//--------------------------------------------------------------------------------------
// Class encapsulating a model
//--------------------------------------------------------------------------------------
// Holds a pointer to a mesh as well as position, rotation and scaling, which are converted to a world matrix when required
// This is more of a convenience class, the Mesh class does most of the difficult work.

#include "Common.h"
#include "CVector3.h"
#include "CMatrix4x4.h"
#include "Input.h"

#ifndef _MODEL_H_INCLUDED_
#define _MODEL_H_INCLUDED_




class Mesh;
constexpr float Cube_Coll = 6.0f;


class Model
{
private:
	void UpdateWorldMatrix();

	Mesh* mMesh;

	// Position, rotation and scaling for the model
	CVector3 mPosition;
	CVector3 mRotation;
	CVector3 mScale;

	// World matrix for the model - built from the above
	CMatrix4x4 mWorldMatrix;

	void initiateNodeCount();

	inline void CollidingFaceCheck(NodeData* p1, NodeData* p2, CVector3* p_WorldPos, planeData* Current)
	{
		CVector3 CollisionPoint;

		//Input the starting and ending segments, with their world positions, aswell as the local positions of 'this' model as the 
		//Position can be accessed internally.

		//Collision point is a returned variable used to calculate the point of collision.
		bool IsIntercept =
			isTriangleColl(*p_WorldPos + p1->BasicData.Position, *p_WorldPos + p2->BasicData.Position, Current, CollisionPoint);
		if (IsIntercept)
		{
			
			//If there is already a collision and it's intercepting with another point then average the point between the collisions.

				++p2->delayChange;
			//	p2->Old_Position = p2->BasicData.Position;
				p2->ReboundForce +=
					(CVector3((Current->a->BasicData.Position + Position()) * CollisionPoint.z) + // Point A * U
					CVector3((Current->b->BasicData.Position + Position()) * CollisionPoint.y) + // Point B * V
					CVector3((Current->c->BasicData.Position + Position()) * CollisionPoint.x)) -  // Point C * W
					*p_WorldPos;
				p2->ReboundForce *= .75f;
				//This needs to be the collision point instead of the actual collisions (But slightly away)

		}
		
	}

	inline bool isWithinRange(Model* CollPos);

	inline bool isWithinRange(float CollPos, float OrigPos, float PolyPos);
	int CollidedVertexSize;

	//A heavily used function that needs to be oxptimized for as much efficiency as possible.
	//Include as many possible returns or GOTO's in order to increase efficiency.
	inline bool isTriangleColl(CVector3 Ray, CVector3 Collide, planeData* Face, CVector3 &CollPoint);
public:
	//-------------------------------------
	// Construction / Usage
	//-------------------------------------

	Model(Mesh* mesh, CVector3 position = { 0,0,0 }, CVector3 rotation = { 0,0,0 }, float scale = 1)
		: mMesh(mesh), mPosition(position), mRotation(rotation), mScale({ scale, scale, scale })
	{
		initiateNodeCount();
	}

	// The render function sets the world matrix in the per-frame constant buffer and makes that buffer available
	// to vertex & pixel shader. Then it calls Mesh:Render, which renders the geometry with current GPU settings.
	// So all other per-frame constants must have been set already along with shaders, textures, samplers, states etc.
	void Render();


	// Control the model's position and rotation using keys provided. Amount of motion performed depends on frame time
	void Control(float frameTime, KeyCode turnUp, KeyCode turnDown, KeyCode turnLeft, KeyCode turnRight,
		KeyCode turnCW, KeyCode turnCCW, KeyCode moveForward, KeyCode moveBackward);


	void FaceTarget(CVector3 target)
	{
		UpdateWorldMatrix();
		mWorldMatrix.FaceTarget(target);
		mRotation = mWorldMatrix.GetEulerAngles();
	}


	//-------------------------------------
	// Data access
	//-------------------------------------

	// Getters / setters
	CVector3 Position() { return mPosition; }
	CVector3 Rotation() { return mRotation; }
	CVector3 Scale() { return mScale; }

	void SetPosition(CVector3 position);
	void SetRotation(CVector3 rotation) { mRotation = rotation; }

	// Two ways to set scale: x,y,z separately, or all to the same value
	void SetScale(CVector3 scale) { mScale = scale; }
	void SetScale(float scale) { mScale = { scale, scale, scale }; }

	// Read only access to model world matrix, updated on request
	CMatrix4x4 WorldMatrix() { UpdateWorldMatrix();  return mWorldMatrix; }

	//MOve this over to the Mesh - The parents don't update otherwise and return null
	CVector3 getSpring(int index, bool isDist);
	CVector3 getSpringFacing(int index, int parentID);

	void isCollision(Model* collider);
	
	int GetVectorMax();
	int GetNullParentVectorMax();

	CVector3 GetCollisionVectors(int i);
	//-------------------------------------
	// Private data / members
	//-------------------------------------

};


#endif //_MODEL_H_INCLUDED_
