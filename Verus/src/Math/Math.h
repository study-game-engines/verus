// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_PI  3.141592654f
#define VERUS_2PI 6.283185307f
#define VERUS_E   2.718281828f
#define VERUS_GR  1.618034f

#define VERUS_HERMITE_CIRCLE ((0.707107f-0.5f)*8.f)

#define VERUS_FLOAT_THRESHOLD 1e-4f

namespace verus
{
	enum class Relation : int
	{
		outside,
		inside,
		intersect
	};

	enum class Continue : int
	{
		no,
		yes
	};

	enum class Easing : int
	{
		none,

		sineIn,
		sineOut,
		sineInOut,

		quadIn,
		quadOut,
		quadInOut,

		cubicIn,
		cubicOut,
		cubicInOut,

		quartIn,
		quartOut,
		quartInOut,

		quintIn,
		quintOut,
		quintInOut,

		expoIn,
		expoOut,
		expoInOut,

		circIn,
		circOut,
		circInOut,

		backIn,
		backOut,
		backInOut,

		elasticIn,
		elasticOut,
		elasticInOut,

		bounceIn,
		bounceOut,
		bounceInOut,

		count
	};

	namespace VMath = Vectormath::SSE;
}

#include "Vector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Sphere.h"
#include "Bounds.h"
#include "Plane.h"
#include "Frustum.h"
#include "TangentSpaceTools.h"
#include "QuadtreeIntegral.h"
#include "Quadtree.h"
#include "Octree.h"

namespace verus
{
	namespace Math
	{
		struct Pose
		{
			Quat   _orientation = Quat(0);
			Point3 _position = Point3(0);

			Pose() {}
			Pose(const XrPosef& that) : _orientation(that.orientation), _position(that.position) {}
		};
		VERUS_TYPEDEFS(Pose);

		// Bits:
		bool IsPowerOfTwo(int x);
		UINT32 NextPowerOfTwo(UINT32 x);
		int HighestBit(int x);
		int LowestBit(int x);
		bool IsNaN(float x);

		// Angles:
		float ToRadians(float deg);
		float ToDegrees(float rad);
		float WrapAngle(float rad);

		// Interpolation, splines:
		float Lerp(float a, float b, float t);
		float LerpAngles(float a, float b, float t);
		float SmoothStep(float a, float b, float t);
		float ApplyEasing(Easing easing, float x);
		Easing EasingFromString(CSZ s);
		CSZ EasingToString(Easing easing);
		Quat NLerp(float t, RcQuat qA, RcQuat qB);

		// Barycentric:
		Vector3 Barycentric(RcPoint3 a, RcPoint3 b, RcPoint3 c, RcPoint3 p);
		bool IsPointInsideBarycentric(RcVector3 bc);
		template<typename T>
		T BarycentricInterpolation(const T& a, const T& b, const T& c, RcVector3 bc)
		{
			const T ab = b - a;
			const T ac = c - a;
			return a + ab * static_cast<float>(bc.getX()) + ac * static_cast<float>(bc.getY());
		}

		// Shapes:
		Vector3 TriangleNormal(RcPoint3 a, RcPoint3 b, RcPoint3 c);
		float TriangleArea(
			const glm::vec3& a,
			const glm::vec3& b,
			const glm::vec3& c);
		bool IsPointInsideTriangle(
			const glm::vec2& a,
			const glm::vec2& b,
			const glm::vec2& c,
			const glm::vec2& p);

		// Geometry:
		int StripGridIndexCount(int polyCountWidth, int polyCountHeight);
		void CreateStripGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices);
		void CreateListGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices);
		bool CheckIndexBuffer(Vector<UINT16>& vIndices, int maxIndex);

		// Scene:
		Transform3 BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx);
		Transform3 BoundsBoxMatrix(RcPoint3 mn, RcPoint3 mx);
		float ComputeOnePixelDistance(float objectSize, float viewportHeightInPixels = 135, float fovY = VERUS_PI / 4);
		float ComputeDistToMipScale(float texHeight, float viewportHeightInPixels, float objectSize, float fovY);
		void Quadrant(const int** ppSrcMinMax, int** ppDestMinMax, int half, int id);

		int ComputeMipLevels(int w, int h, int d = 1);

		BYTE CombineOcclusion(BYTE a, BYTE b);

		// Matrices:
		Transform3 QuadMatrix(float x = 0, float y = 0, float w = 1, float h = 1);
		Transform3 ToUVMatrix(float zOffset = 0, RcVector4 texSize = Vector4(0), PcVector4 pTileSize = nullptr, float uOffset = 0, float vOffset = 0);

		float Reduce(float val, float reduction);

		Point3 ClosestPointOnSegment(RcPoint3 segA, RcPoint3 segB, RcPoint3 point);
		float SegmentToPointDistance(RcPoint3 segA, RcPoint3 segB, RcPoint3 point);

		void Test();
	};
}
