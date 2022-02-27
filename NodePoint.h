#pragma once

#include "Common.h"
#include "SpringPoint.h"



class Node
{
public:
	void addNode(NodeData input, float NodeMass, bool positionLock);
	

	void setOriginPoint(CVector3 input)
	{
	
		modelPosition = input;
	}

	bool isRoot(int index)
	{
		return (VertexData[index]->root == NULL);
	}

	//Gets the root nodes positions
	CVector3 getPosition(int index)
	{
	return getRoot(index)->BasicData.Position;
	}

	//Get the distance to the last position
	CVector3 getVelocity(int index)
	{
		return getRoot(index)->Velocity;
	}

	void setPosition(CVector3 input, int index)
	{
		if (index < VertexData.size())
		{
			getRoot(index)->BasicData.Position = input;
		}
		UpdateRootChild(index);
	}

	//This function is used as a delayed position setter. Allowing other values to use the outdated data.
	void addForce(CVector3 input, int index)
	{
		if (index < VertexData.size())
		{
			getRoot(index)->ReboundForce = input;
		}
		UpdateRootChild(index);
	}

	//Grabs the indexed nodes root.
	NodeData* getNode(int index)
	{
		return getRoot(index);
	}
	//Grabs the indexed nodes root.
	NodeData* GetChildNode(int index)
	{
		return VertexData[index];
	}

	//Grabs the current parent of a node.
	NodeData* getParent(int index)
	{
		return VertexData[index]->root;
	}


	int getSize()
	{
		return VertexData.size();
	}

	//Go through the nodeList and calculate the force being put on each node
	void applyForce(float updateTime, CVector3 externalForces);

	//Updates the size of the current amount of 'Root' nodes.
	void setupRootSize()
	{
		RootVertexSize = 0;
		int i = 0;
		while (i < VertexData.size())
		{
			if (VertexData[i]->root == NULL)
			{
				++RootVertexSize;
			}
			++i;
		}
		i = i;
	}

	//Gets the size of the nodes with a parent of NULL
	int getRootSize()
	{
		return RootVertexSize;
	}

	//Adds 3 nodes together and into a face.
	//Typically used for collisions.
	void addFace(NodeData* a, NodeData* b, NodeData* c)
	{
		planeData input;

		input.a = a;
		input.b = b;
		input.c = c;

		/*
		//This is a possibility with unmoving objects but with a 
		//Soft body the positions will need to be updated consistantly.
		//A cross product is expensive and so this should be avoided.

		input.normal = Normalise(Cross
		(b->BasicData.Position - a->BasicData.Position, 
			c->BasicData.Position - a->BasicData.Position));

		input.dot = Dot(input.normal, a->BasicData.Position);
		
		*/
		FaceList.push_back(input);
	}


	//This will be used mainly to gain easy access to the face
	//Order when doing collisions. 
	planeData* getFace(int index)
	{
		return &FaceList[index];
	}

	int getFaceSize()
	{
		return FaceList.size();
	}

	//Sets the node positions back to their origins. Effectively reseting a simulation.
	void resetPoints()
	{
		for (int i = 0; i < VertexData.size(); ++i)
		{
			VertexData[i]->BasicData.Position = BaseData[i]->BasicData.Position;
			VertexData[i]->Old_Position = BaseData[i]->Old_Position;
			VertexData[i]->Velocity = BaseData[i]->Velocity;
		}
	}


	//Delete allocated memory.
	~Node()
	{
		for (int i = 0; i < VertexData.size(); ++i)
		{
			delete VertexData[i];
			delete BaseData[i];
		}
	}
private:
	std::vector<NodeData*> BaseData; //Origin list for the nodes to revert to when simulation is reset
	std::vector<NodeData*> VertexData; //List of nodes and their current position
	std::vector<planeData> FaceList; //List of faces in the above trees, linked directly to the vertexData nodes.
	int RootVertexSize;

	CVector3 modelPosition; //Gives the node access to the models position so it can calculate world positions.

	//Disconnect/Connect faces
	const bool ifFaceConnect = true;

	//Grab the parent of the current node.
	//This is done by going through each of its parents until it finds the root
	NodeData* getRoot(int index)
	{
		if (index < VertexData.size())
		{
			NodeData* root = VertexData[index];
			if (ifFaceConnect)
			{
				while (root->root != NULL)
				{
					root = root->root;
				}
			}
			return root;
		}
		return VertexData.back();
	}



	//The root position will have all the calculations done to it. 
	//As the children are bound to the root position they will need to be updated.
	void UpdateRootChild(int index)
	{
		if (ifFaceConnect)
		{
			NodeData* current = VertexData[index];
			NodeData* root = getRoot(index);

			current->BasicData.Position = root->BasicData.Position;
			current->isBound = root->isBound;
			current->Old_Position = root->Old_Position;
			current->SpringList = root->SpringList;
			current->Velocity = root->Velocity;
			current->_Mass = root->_Mass;

		}
	}



};

