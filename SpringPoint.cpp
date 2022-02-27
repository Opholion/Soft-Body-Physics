#include "SpringPoint.h"

SpringPoint::SpringPoint(NodeData& input_A, NodeData& input_B, float input_Bleed)
{
	SpringCoefficient = input_Bleed;

	//Assign each parent as an input.
	Parents[0] = &input_A;
	Parents[1] = &input_B;

	//For the lengthdiff, get the distance between each parent and take away the intertial length
	m_InertialLength = getDist(&Parents[0]->BasicData.Position, &Parents[1]->BasicData.Position);

	//Make sure that m_IntertialLength is initialized first.
	//Allowing each section a connection to the spring/node they're bound to.
	
	//Iterate through each parent.
	NodeData* root[2];
	for (int i = 0; i < 2; ++i)
	{
		//Make sure that they are the root node that each child node defers to.
		root[i] = Parents[i];
		//Root nodes can be found by checking if their root is equal to 0 (NULL)
		while (root[i]->root != NULL)
		{
			root[i] = root[i]->root;
		}

		//Once the root node is found this spring will be added to their personal list of springs.
		//This is used for the final force calculation
		root[i]->SpringList.push_back(this);
	}

	root[0]->ConnectedNodes.push_back(root[1]);
	root[1]->ConnectedNodes.push_back(root[0]);
}
const float groundHeight = -15.0f;
//Alright. If the node is in the same position as another then they must be the edge of a face. Thus if they have the same position then point them to the same node.
CVector3 SpringPoint::calculateForce(NodeData* particle)
{

	// Return zero force if spring not attached to given particle, or if given particle is pinned
	//...
	if (&Parents[0]->BasicData == nullptr || &Parents[1]->BasicData == nullptr)
	{
		return CVector3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		// Calculate strength of force based on current spring length and inertial length
		float forceStrength;
		CVector3 direction;

		//There is a drift. This will be due to the structure of the springs
		CVector3 Pos1 = Parents[0]->BasicData.Position;
		CVector3 Pos2 = Parents[1]->BasicData.Position;
	
		//direction of spring, this will effect the IF statement, returning a negative for the other position.
		direction = Pos1 - Pos2;


		float currLength = getDist(&Pos1, &Pos2);

		float inertialLength = m_InertialLength;

		forceStrength = SpringCoefficient * (currLength - inertialLength);
		

		CVector3 preOutput = direction * forceStrength;
		CVector3 Output = preOutput / currLength;

		timeGetTime();
		if (particle == Parents[1])
		{
			return 1.0f * Output;
		}
		return -1.0f * Output;

	}

	return CVector3(0, 0, 0);
}

	