//--------------------------------------------------------------------------------------
// Class encapsulating a model
//--------------------------------------------------------------------------------------
// Holds a pointer to a mesh as well as position, rotation and scaling, which are converted to a world matrix when required
// This is more of a convenience class, the Mesh class does most of the difficult work.

#include "Model.h"

#include "Common.h"
#include "GraphicsHelpers.h"
#include "Mesh.h"

void Model::SetPosition(CVector3 position) { mPosition = position; mMesh->VertexData.setOriginPoint(mPosition); }

void Model::initiateNodeCount()
{
	mMesh->VertexData.setOriginPoint(mPosition);
	CollidedVertexSize = mMesh->VertexData.getSize();//Couldn't figure out how to turn Models into a external contructor so this is the way.
	//mMesh references need to be outside the class.h
}


void Model::Render()
{
	
    UpdateWorldMatrix();

    gPerModelConstants.worldMatrix = mWorldMatrix; // Update C++ side constant buffer
    UpdateConstantBuffer(gPerModelConstantBuffer, gPerModelConstants); // Send to GPU

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(1, 1, &gPerModelConstantBuffer); // First parameter must match constant buffer number in the shader
    gD3DContext->PSSetConstantBuffers(1, 1, &gPerModelConstantBuffer);

	mMesh->Render();
}



// Control the model's position and rotation using keys provided. Amount of motion performed depends on frame time
void Model::Control(float frameTime, KeyCode turnUp, KeyCode turnDown, KeyCode turnLeft, KeyCode turnRight,
                                     KeyCode turnCW, KeyCode turnCCW, KeyCode moveForward, KeyCode moveBackward)
{
    UpdateWorldMatrix();

	if (KeyHeld( turnDown ))
	{
		mRotation.x += ROTATION_SPEED * frameTime;
	}
	if (KeyHeld( turnUp ))
	{
		mRotation.x -= ROTATION_SPEED * frameTime;
	}
	if (KeyHeld( turnRight ))
	{
		mRotation.y += ROTATION_SPEED * frameTime;
	}
	if (KeyHeld( turnLeft ))
	{
		mRotation.y -= ROTATION_SPEED * frameTime;
	}
	if (KeyHeld( turnCW ))
	{
		mRotation.z += ROTATION_SPEED * frameTime;
	}
	if (KeyHeld( turnCCW ))
	{
		mRotation.z -= ROTATION_SPEED * frameTime;
	}

	// Local Z movement - move in the direction of the Z axis, get axis from world matrix
    CVector3 localZDir = Normalise({ mWorldMatrix.e20, mWorldMatrix.e21, mWorldMatrix.e22 }); // normalise axis in case world matrix has scaling
	if (KeyHeld( moveForward ))
	{
		mPosition.x += localZDir.x * MOVEMENT_SPEED * frameTime;
		mPosition.y += localZDir.y * MOVEMENT_SPEED * frameTime;
		mPosition.z += localZDir.z * MOVEMENT_SPEED * frameTime;
	}
	if (KeyHeld( moveBackward ))
	{
		mPosition.x -= localZDir.x * MOVEMENT_SPEED * frameTime;
		mPosition.y -= localZDir.y * MOVEMENT_SPEED * frameTime;
		mPosition.z -= localZDir.z * MOVEMENT_SPEED * frameTime;
	}
}


void Model::UpdateWorldMatrix()
{
    mWorldMatrix = MatrixScaling(mScale) * MatrixRotationZ(mRotation.z) * MatrixRotationX(mRotation.x) * MatrixRotationY(mRotation.y) * MatrixTranslation(mPosition);
}


const float ModelWidth = 400.0f;

void Model::isCollision(Model* collider)
{
	CVector3 Coll_Scale = collider->mScale;
	//If within X box size then check if the vertices are colliding
	if (isWithinRange(collider))
	{
		CVector3 ColliderPosition = collider->Position(); //Increases memory usage but decreases time taken to fetch data.
		planeData* CurrentMod;


		planeData* Collider;

		int ColliderVertexSize = collider->mMesh->VertexData.getFaceSize();
		CollidedVertexSize = mMesh->VertexData.getFaceSize();

		for (unsigned int i = 0; i < CollidedVertexSize
			; ++i)//This is for each corner of the triangle. Instead of incrementing by 2 or 3 it increments by 1 in order to use the previous 2 positions to build another 2D triangle. 
		{
			//Needs faces to create a working collision triangle
			CurrentMod = mMesh->VertexData.getFace(i);

			for (unsigned int j = 0; j < ColliderVertexSize; ++j)
			{
				//Only needs the point so it's more efficient to iterate through the root nodes as faces will have overlap.
				Collider = collider->mMesh->VertexData.getFace(j);

				//If DelayChange is true then the problem has been addressed. 
				CollidingFaceCheck(Collider->b, Collider->a,&ColliderPosition, CurrentMod);
				CollidingFaceCheck(Collider->c, Collider->b,&ColliderPosition, CurrentMod);
				CollidingFaceCheck(Collider->a, Collider->c,&ColliderPosition, CurrentMod);			
			}
			

		}
	 }
}

 CVector3 Model::GetCollisionVectors(int i)
 {
	 return mMesh->VertexData.getPosition(i);
 }

 int Model::GetVectorMax()
 {
	 return mMesh->VertexData.getSize();
 }

 int Model::GetNullParentVectorMax()
 {
	 return mMesh->VertexData.getRootSize();
 }

 inline bool Model::isWithinRange(Model* CollPos)
 {
	
	 
	 float distance = Distance(CollPos->Position(), this->Position());

	 //Dot(CollPos,OrigPos)
     //Dot(OrigPos + PolyPos,OrigPos)



	 if (10.0f * CollPos->Scale().x < distance)
	 {
		 return true;
	 }

	 return false;
 }

 inline bool Model::isWithinRange(float CollPos, float OrigPos, float PolyPos)
 {
	 if (PolyPos > 0)
	 {
		 if (CollPos >= OrigPos && CollPos <= OrigPos + PolyPos)
		 {
			 return true;
		 }
	 }
	 else
	 {
		 if (CollPos <= OrigPos && CollPos >= OrigPos + PolyPos)
		 {
			 return true;
		 }
	 }
	 return false;
 }


 CVector3 Model::getSpring(int index, bool isDist)
 {
	 if (index < mMesh->getSpringSize())
	 {
		 CVector3 parent1 = *mMesh->getSpringParent(index, 0);
		 CVector3 parent2 = *mMesh->getSpringParent(index, 1);

		 if (isDist)
		 {
			 return CVector3
			 (
				 abs(parent1.x - parent2.x),
				 abs(parent1.y - parent2.y),
				 abs(parent1.z - parent2.z)
			 );
		 }
		 else
		 {
			 return CVector3 //Getting the point between each parent to label as the springs position.
			 (
				 (parent1.x + parent2.x) / 2.0f,
				 (parent1.y + parent2.y) / 2.0f,
				 (parent1.z + parent2.z) / 2.0f
			 );
		 }
	 }

 }

 CVector3 Model::getSpringFacing(int index, int parentID)
 {
	 if (index < mMesh->getSpringSize())
	 {
		 CVector3 parent = *mMesh->getSpringParent(index, parentID);

		 return CVector3 //Getting the point between each parent to label as the springs position.
		 (
			 parent
		 );
	 }

 }


 inline bool Model::isTriangleColl(CVector3 Collide0, CVector3 Collide1, planeData* Face, CVector3 &CollPoint)
 {
	 //Collide01 have their positions preloaded for global calculations. Anything from face will be local to Position();
	 //As we're only getting the point between a and b or a and c, we only need to get the position once per calculation.
		 CVector3 PntAB = Position() +(Face->b->BasicData.Position - Face->a->BasicData.Position);
		 CVector3 PntAC = Position() +(Face->c->BasicData.Position - Face->a->BasicData.Position);
		 CVector3 CollideAB = Collide0 - Collide1;

		 CVector3 n = Cross(PntAB, PntAC);

		 float d = Dot(CollideAB,n);
		 if (d <= .0f) { return 0; } //If less than 0 then it is parralel or moving away. Force it to leave early for optimization

		 CVector3 PntAP = Collide0 - (Position() + Face->a->BasicData.Position);
		 float t = Dot(PntAP, n);
		 if (t < .0f) { return 0; }
		 if (t > d) { return 0; } //Check if it's within range. This is the part of the code which makes it a segment instead of a ray.

		 //Test if held wthin bounds after grabbing the barycentric coordinates.
		 CVector3 e = Cross(CollideAB, PntAP);
		 float v = Dot(PntAC,e);
		 if (v < .0f || v > d)
		 {
			 return 0;
		 }
		 float w = -Dot(PntAB, e);
		 if (w < .0f || v + w > d) { return 0; }


		 //Here we calculate the collision point in babycentric coordinates./=
		 float ood = 1.f / d;
		 //t *= ood;
		 CollPoint.z = w * ood; //V 
		 CollPoint.y = v * ood; //W
		 CollPoint.x = 1.0f - CollPoint.y - CollPoint.z; //U

		 //If there isn't a collision then grab the distance of Collide to the plane.
		 //Segments are represented as point a and point b, or in this case, p and q. 

	 return true;
 }
