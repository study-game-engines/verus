#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// SceneNode:

SceneNode::SceneNode()
{
}

SceneNode::~SceneNode()
{
}

int SceneNode::UserPtr_GetType()
{
	return +_type;
}

void SceneNode::DrawBounds()
{
	if (!_bounds.IsNull())
	{
		VERUS_QREF_HELPERS;
		helpers.DrawBox(&_bounds.GetDrawTransform());
	}
}

void SceneNode::SetTransform(RcTransform3 tr)
{
	_tr = tr;
	// Also update the UI values and bounds:
	const Quat q(_tr.getUpper3x3());
	_uiRotation.EulerFromQuaternion(q);
	_uiScale = _tr.GetScale();
	UpdateBounds();
}

void SceneNode::ComputeTransform()
{
	Quat q;
	_uiRotation.EulerToQuaternion(q);
	_tr = VMath::appendScale(Transform3(q, _tr.getTranslation()), _uiScale);
}

Point3 SceneNode::GetPosition() const
{
	return _tr.getTranslation();
}

Vector3 SceneNode::GetRotation() const
{
	return _uiRotation;
}

Vector3 SceneNode::GetScale() const
{
	return _uiScale;
}

void SceneNode::MoveTo(RcPoint3 pos)
{
	_tr.setTranslation(Vector3(pos));
	UpdateBounds();
}

void SceneNode::RotateTo(RcVector3 v)
{
	_uiRotation = v;
	ComputeTransform();
	UpdateBounds();
}

void SceneNode::ScaleTo(RcVector3 v)
{
	_uiScale = v;
	ComputeTransform();
	UpdateBounds();
}

void SceneNode::RemoveRigidBody()
{
	if (_pBody)
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pBody);
		delete _pBody->getMotionState();
		delete _pBody;
		_pBody = nullptr;
	}
}

void SceneNode::SetCollisionGroup(Physics::Group g)
{
	if (_pBody && _pBody->getBroadphaseHandle()->m_collisionFilterGroup != +g)
	{
		// http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=7538
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pBody);
		bullet.GetWorld()->addRigidBody(_pBody, +g, -1);
	}
}

void SceneNode::MoveRigidBody()
{
	if (_pBody)
	{
		// http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=6729
		VERUS_QREF_BULLET;
		_pBody->setWorldTransform(_tr.Bullet());
		bullet.GetWorld()->updateSingleAabb(_pBody);
	}
}

void SceneNode::SaveXML(pugi::xml_node node)
{
	node.append_attribute("name") = _C(_name);
	node.append_attribute("uiR") = _C(_uiRotation.ToString(true));
	node.append_attribute("uiS") = _C(_uiScale.ToString(true));
	node.append_attribute("m") = _C(_tr.ToString());
}

void SceneNode::LoadXML(pugi::xml_node node)
{
	if (auto attr = node.attribute("name"))
		_name = attr.value();
	_uiRotation.FromString(node.attribute("uiR").value());
	_uiScale.FromString(node.attribute("uiS").value());
	_tr.FromString(node.attribute("m").value());
}

float SceneNode::GetDistToEyeSq() const
{
	VERUS_QREF_SM;
	return VMath::distSqr(sm.GetCamera()->GetEyePosition(), GetPosition());
}