#include "NodePoint.h"


const float groundHeight = -.0f;

void Node::addNode(NodeData input, float NodeMass, bool positionLock)
{

	input._Mass = NodeMass;
	input.Velocity = CVector3(.0f, .0f, .0f);
	input.Old_Position = input.BasicData.Position;
	input.isBound = positionLock;
	input.root = NULL;
	VertexData.push_back(new NodeData(input));
	BaseData.push_back(new NodeData(input));
	//Bind face edges together. Everything should access the parent. 
	//Go through all but .back() in VertexData
	for (int i = 0; i < VertexData.size() - 1; ++i)
	{
		//Basic, if inefficient, safety check for floating point errors.
		CVector3 ifEqual = VertexData[i]->BasicData.Position - VertexData.back()->BasicData.Position;
		if (abs(ifEqual.x) <= 0.01f && abs(ifEqual.y) <= 0.01f && abs(ifEqual.z) <= 0.01f)
		{
			VertexData.back()->root = VertexData.at(i);
		}
	}

}

void Node::applyForce(float updateTime, CVector3 externalForces)
{


	for (int i = 0; i < VertexData.size(); ++i)
	{

		//The root parents should hold all the springs. No other node calculations matter.
		//The first instances of points will be the roots. Everything must be updated from 0-limit
		if (VertexData[i]->root == NULL)
		{
			//This will act as the holder of the forces involved in the current node.
			CVector3 internalForces = { 0, 0, 0 };

			for (int j = 0; j < VertexData[i]->SpringList.size(); ++j) 
			{
				internalForces += VertexData[i]->SpringList[j]->calculateForce(std::move(getRoot(i)));
			}

			internalForces += externalForces; //Adding constant, static, forces - such as wind/gravity


			//Bound nodes are more sturdy and will need to adjust to models of different
			//complexity.
			if (VertexData[i]->isBound)
			{
				internalForces *= 5;
				VertexData[i]->_Mass = VertexData[i]->SpringList.size() / 5.f;		
			}



			//Mass acts as a form of resistance to outside forces, making it more stubborn
			//A node with more mass will start pushing other nodes before itself.
			internalForces = internalForces / VertexData[i]->_Mass; //Now acceleration


			//Instead of using collisions on the floor object this is used to 
			//Require less processing speed. Delete this if intended to be used
			//Outside of the showcase.
			if (!VertexData[i]->isBound)
			{
				if (VertexData[i]->BasicData.Position.y<= (groundHeight- modelPosition.y) + 0.1f)
				{
					if (internalForces.y < 0.)
					{
						internalForces.y = 0.;
					}
					if (VertexData[i]->BasicData.Position.y < (groundHeight- modelPosition.y))
					{
						VertexData[i]->BasicData.Position.y = (groundHeight- modelPosition.y);
					}

				}
			}

			float damp = pow(0.0015f, updateTime);
			CVector3 FuturePos = (1.0 + damp) * VertexData[i]->BasicData.Position - damp *
				VertexData[i]->Old_Position + internalForces * updateTime * updateTime;




			//Make virtuals for nodes
			//Look into making a shape in the center of the object to add different lengths to springs.
			VertexData[i]->Old_Position = VertexData[i]->BasicData.Position;

			VertexData[i]->Velocity = FuturePos - VertexData[i]->Old_Position;


			//Delay change is a function used to delay changes to allow
			//Calculations to catch up.
			//Done as a boolean to make the IF statement use a faster and
			//More basic check.
			if (VertexData[i]->delayChange >= 0.9f)
			{
				VertexData[i]->ReboundForce = VertexData[i]->ReboundForce / (float)VertexData[i]->delayChange;
				VertexData[i]->delayChange = 0;
				VertexData[i]->BasicData.Position = VertexData[i]->ReboundForce;
				VertexData[i]->ReboundForce = CVector3(.0f, .0f, .0f);
			}else
		    //If the node is bound then it should be more naturally resistant.
			if (VertexData[i]->isBound)
			{
				VertexData[i]->BasicData.Position = (FuturePos + VertexData[i]->Old_Position) * 0.500001f;
			}
			else
			{
				VertexData[i]->BasicData.Position = FuturePos;
			}


		}
		else
		{
			//Make all children equal to the parent.
			//This will apply to the children and so it must be applied to every child.
			UpdateRootChild(i);
		}
	}



}