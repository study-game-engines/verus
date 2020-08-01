#include "verus.h"

using namespace verus;
using namespace verus::Scene;

SceneManager::SceneManager()
{
	VERUS_ZERO_MEM(_visibleCountPerType);
}

SceneManager::~SceneManager()
{
	Done();
}

void SceneManager::Init(RcDesc desc)
{
	VERUS_INIT();

	SetCamera(desc._pMainCamera);

	const float hf = desc._mapSide * 0.5f;
	Math::Bounds bounds;
	bounds.Set(
		Vector3(-hf, -hf, -hf),
		Vector3(+hf, +hf, +hf));
	const Vector3 limit(hf / 14, hf / 14, hf / 14);
	_octree.Done();
	_octree.Init(bounds, limit);
	_octree.SetDelegate(this);
}

void SceneManager::Done()
{
	{ // PBLE:
		//DeleteAllPrefabs();
		DeleteAllBlocks();
		DeleteAllLights();
		//DeleteAllEmitters();
	}
	DeleteAllModels();
	VERUS_DONE(SceneManager);
}

void SceneManager::ResetInstanceCount()
{
	for (auto& x : TStoreModels::_map)
		x.second.GetMesh().ResetInstanceCount();
}

void SceneManager::Update()
{
	VERUS_UPDATE_ONCE_CHECK;
	//for (auto& x : TStoreSites::_map)
	//	x.second.Update();
	{ // PBLE:
		//for (auto& x : TStorePrefabs::_list)
		//	x.Update();
		for (auto& x : TStoreBlocks::_list)
			x.Update();
		for (auto& x : TStoreLights::_list)
			x.Update();
		//for (auto& x : TStoreEmitters::_list)
		//	x.Update();
	}
	//for (auto& x : TStoreSceneParticles::_map)
	//	x.second.Update();
}

void SceneManager::UpdateParts()
{
	const RcPoint3 eyePos = _pMainCamera->GetEyePosition();
	for (auto& block : TStoreBlocks::_list)
	{
		const float distSq = VMath::distSqr(block.GetPosition(), eyePos);
		const float part = MaterialManager::ComputePart(distSq, block.GetBounds().GetAverageSize() * 0.5f);
		block.GetMaterial()->IncludePart(part);
	}
}

void SceneManager::Layout()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_ATMO;
	VERUS_QREF_SM;

	// Allocate enough space:
	int countAll = 0;
	//countAll += TStorePrefabs::_list.size();
	countAll += TStoreBlocks::_list.size();
	countAll += TStoreLights::_list.size();
	//countAll += TStoreSites::_map.size();
	if (_vVisibleNodes.size() != countAll)
		_vVisibleNodes.resize(countAll);

	_visibleCount = 0;
	VERUS_ZERO_MEM(_visibleCountPerType);

	PCamera pPrevCamera = nullptr;
	// For CSM we need to create geometry beyond the view frustum (1st slice):
	if (settings._sceneShadowQuality >= App::Settings::ShadowQuality::cascaded && atmo.GetShadowMap().IsRendering())
	{
		PCamera pCameraCSM = atmo.GetShadowMap().GetCameraCSM();
		if (pCameraCSM)
			pPrevCamera = sm.SetCamera(pCameraCSM);
	}
	_octree.TraverseVisible(_pCamera->GetFrustum());
	// Back to original camera:
	if (pPrevCamera)
		SetCamera(pPrevCamera);

	VERUS_RT_ASSERT(!_visibleCountPerType[+NodeType::unknown]);
	std::sort(_vVisibleNodes.begin(), _vVisibleNodes.begin() + _visibleCount, [](PSceneNode pA, PSceneNode pB)
		{
			// Sort by node type?
			const NodeType typeA = pA->GetType();
			const NodeType typeB = pB->GetType();
			if (typeA != typeB)
				return typeA < typeB;

			// Both are blocks?
			if (NodeType::block == typeA)
			{
				PBlock pBlockA = static_cast<PBlock>(pA);
				PBlock pBlockB = static_cast<PBlock>(pB);
				MaterialPtr materialA = pBlockA->GetMaterial();
				MaterialPtr materialB = pBlockB->GetMaterial();

				if (materialA && materialB) // A and B have materials, compare them.
				{
					const bool ab = *materialA < *materialB;
					const bool ba = *materialB < *materialA;
					if (ab || ba)
						return ab;
				}
				else if (materialA) // A is with material, B is without, so A goes first.
				{
					return true;
				}
				else if (materialB) // A is without material, B is with, so B goes first.
				{
					return false;
				}

				// Equal materials or none have any material, compare models used:
				ModelPtr modelA = pBlockA->GetModel();
				ModelPtr modelB = pBlockB->GetModel();
				if (modelA != modelB)
					return modelA < modelB;
			}

			// Both are lights?
			if (NodeType::light == typeA)
			{
				PLight pLightA = static_cast<PLight>(pA);
				PLight pLightB = static_cast<PLight>(pB);
				const CGI::LightType lightTypeA = pLightA->GetLightType();
				const CGI::LightType lightTypeB = pLightB->GetLightType();
				if (lightTypeA != lightTypeB)
					return lightTypeA < lightTypeB;
			}

			// Draw same node types front-to-back:
			return pA->GetDistToEyeSq() < pB->GetDistToEyeSq();
		});
}

void SceneManager::Draw()
{
	if (!_visibleCountPerType[+NodeType::block])
		return;

	VERUS_QREF_RENDERER;

	ModelPtr model;
	MaterialPtr material;
	bool bindPipeline = true;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Scene::Mesh::GetShader();

	const int begin = 0;
	const int end = _visibleCountPerType[+NodeType::block];
	shader->BeginBindDescriptors();
	for (int i = begin; i <= end; ++i)
	{
		if (i == end)
		{
			if (model)
				model->Draw(cb);
			break;
		}

		PSceneNode pSN = _vVisibleNodes[i];
		PBlock pBlock = static_cast<PBlock>(pSN);
		ModelPtr nextModel = pBlock->GetModel();
		MaterialPtr nextMaterial = pBlock->GetMaterial();

		if (!nextModel->IsLoaded() || !nextMaterial->IsLoaded())
			continue;

		if (nextModel != model)
		{
			if (model)
				model->Draw(cb);

			model = nextModel;
			model->MarkFirstInstance();
			if (bindPipeline)
			{
				bindPipeline = false;
				model->BindPipeline(cb);
				cb->BindDescriptors(shader, 0);
			}
			model->BindGeo(cb);
			cb->BindDescriptors(shader, 2);
		}
		if (nextMaterial != material)
		{
			material = nextMaterial;
			material->UpdateMeshUniformBuffer();
			cb->BindDescriptors(shader, 1, material->GetComplexSetHandle());
		}

		if (model)
			model->PushInstance(pBlock->GetTransform(), pBlock->GetColor());
	}
	shader->EndBindDescriptors();
}

void SceneManager::DrawLights()
{
	if (!_visibleCountPerType[+NodeType::light])
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_HELPERS;

	CGI::LightType type = CGI::LightType::none;
	PMesh pMesh = nullptr;
	bool bindPipeline = true;

	auto& ds = renderer.GetDS();
	auto cb = renderer.GetCommandBuffer();

	auto DrawMesh = [cb](PMesh pMesh)
	{
		if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
		{
			pMesh->UpdateInstanceBuffer();
			cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
		}
	};

	const int begin = _visibleCountPerType[+NodeType::block];
	const int end = _visibleCountPerType[+NodeType::block] + _visibleCountPerType[+NodeType::light];
	for (int i = begin; i <= end; ++i)
	{
		if (i == end)
		{
			DrawMesh(pMesh);
			break;
		}

		PSceneNode pSN = _vVisibleNodes[i];
		PLight pLight = static_cast<PLight>(pSN);
		const CGI::LightType nextType = pLight->GetLightType();

		if (nextType != type)
		{
			DrawMesh(pMesh);

			type = nextType;
			ds.OnNewLightType(cb, type);

			pMesh = (type != CGI::LightType::none) ? &helpers.GetDeferredLights().Get(type) : nullptr;

			if (pMesh)
			{
				pMesh->MarkFirstInstance();
				pMesh->BindGeo(cb, (1 << 0) | (1 << 4));
				pMesh->CopyPosDeqScale(&ds.GetUbPerMeshVS()._posDeqScale.x);
				pMesh->CopyPosDeqBias(&ds.GetUbPerMeshVS()._posDeqBias.x);
				ds.BindDescriptorsPerMeshVS(cb);
			}
		}

#ifdef VERUS_RELEASE_DEBUG
		if (CGI::LightType::dir == type) // Must not be scaled!
		{
			RcTransform3 matW = pLight->GetTransform();
			const Vector3 s = matW.GetScale();
			VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(s.GLM(), glm::vec3(1), glm::vec3(VERUS_FLOAT_THRESHOLD))));
		}
#endif

		if (pMesh)
			pMesh->PushInstance(pLight->GetTransform(), pLight->GetInstData());
	}
}

bool SceneManager::IsDrawingDepth(DrawDepth dd)
{
	if (DrawDepth::automatic == dd)
	{
		VERUS_QREF_ATMO;
		return atmo.GetShadowMap().IsRendering();
	}
	else
		return DrawDepth::yes == dd;
}

String SceneManager::EnsureUniqueName(CSZ name)
{
	// Compact name:
	const char* s = strrchr(name, ':');
	s = s ? s + 1 : name;
	const char* e = strrchr(name, '.');
	if (!e)
		e = name + strlen(name);

	// Unique name:
	int id = -1;
	String test;
	do
	{
		test = (id < 0) ? String(s, e) : String(s, e) + "_" + std::to_string(id);
		//for (auto& it : TStorePrefabs::_list)	if (it.GetName() == _C(test)) test.clear();
		for (auto& it : TStoreBlocks::_list)	if (it.GetName() == _C(test)) test.clear();
		for (auto& it : TStoreLights::_list)	if (it.GetName() == _C(test)) test.clear();
		//for (auto& it : TStoreEmitters::_list)	if (it.GetName() == _C(test)) test.clear();
		id++;
	} while (test.empty());

	return test;
}

PModel SceneManager::InsertModel(CSZ url)
{
	return TStoreModels::Insert(url);
}

PModel SceneManager::FindModel(CSZ url)
{
	return TStoreModels::Find(url);
}

void SceneManager::DeleteModel(CSZ url)
{
	TStoreModels::Delete(url);
}

void SceneManager::DeleteAllModels()
{
	TStoreModels::DeleteAll();
}

PBlock SceneManager::InsertBlock()
{
	return TStoreBlocks::Insert();
}

void SceneManager::DeleteBlock(PBlock p)
{
	TStoreBlocks::Delete(p);
}

void SceneManager::DeleteAllBlocks()
{
	TStoreBlocks::DeleteAll();
}

PLight SceneManager::InsertLight()
{
	return TStoreLights::Insert();
}

void SceneManager::DeleteLight(PLight p)
{
	TStoreLights::Delete(p);
}

void SceneManager::DeleteAllLights()
{
	TStoreLights::DeleteAll();
}

Continue SceneManager::Octree_ProcessNode(void* pToken, void* pUser)
{
	PSceneNode pSN = static_cast<PSceneNode>(pToken);
	if (pSN->IsHidden())
		return Continue::yes;
	_vVisibleNodes[_visibleCount++] = pSN;
	_visibleCountPerType[+pSN->GetType()]++;
	return Continue::yes;
}

bool SceneManager::RayCastingTest(RcPoint3 pointA, RcPoint3 pointB, PBlockPtr pBlock,
	PPoint3 pPoint, PVector3 pNormal, const float* pRadius, Physics::Group mask)
{
	VERUS_RT_ASSERT(!pBlock || *pBlock);
	VERUS_QREF_BULLET;
	btVector3 from(pointA.Bullet()), to(pointB.Bullet());
	if (pPoint || pNormal)
	{
		if (pRadius)
		{
			btSphereShape sphere(*pRadius);
			btTransform trA, trB;
			trA.setIdentity();
			trB.setIdentity();
			trA.setOrigin(pointA.Bullet());
			trB.setOrigin(pointB.Bullet());
			btCollisionWorld::ClosestConvexResultCallback ccrc(from, to);
			ccrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->convexSweepTest(&sphere, trA, trB, ccrc);
			if (ccrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(ccrc.m_hitCollisionObject->getUserPointer());
				if (pBlock && p->UserPtr_GetType() == +NodeType::block)
					pBlock->Attach(static_cast<PBlock>(p));
				if (pPoint)
					*pPoint = ccrc.m_hitPointWorld;
				if (pNormal)
					*pNormal = ccrc.m_hitNormalWorld;
				return true;
			}
			else
				return false;
		}
		else
		{
			btCollisionWorld::ClosestRayResultCallback crrc(from, to);
			crrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->rayTest(from, to, crrc);
			if (crrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(crrc.m_collisionObject->getUserPointer());
				if (pBlock && p->UserPtr_GetType() == +NodeType::block)
					pBlock->Attach(static_cast<PBlock>(p));
				if (pPoint)
					*pPoint = crrc.m_hitPointWorld;
				if (pNormal)
					*pNormal = crrc.m_hitNormalWorld;
				return true;
			}
			else
				return false;
		}
	}
	else // Point/normal not required?
	{
		struct AnyRayResultCallback : public btCollisionWorld::RayResultCallback
		{
			btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
			{
				m_closestHitFraction = 0;
				m_collisionObject = rayResult.m_collisionObject;
				return 0;
			}
		} arrc;
		arrc.m_collisionFilterMask = +mask;
		bullet.GetWorld()->rayTest(from, to, arrc);
		if (arrc.hasHit())
		{
			Physics::PUserPtr p = static_cast<Physics::PUserPtr>(arrc.m_collisionObject->getUserPointer());
			if (pBlock && p->UserPtr_GetType() == +NodeType::block)
				pBlock->Attach(static_cast<PBlock>(p));
			return true;
		}
		else
			return false;
	}
	return false;
}
