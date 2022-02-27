#pragma once

#include "Common.h"


inline float getDist(CVector3* tempPos1, CVector3* tempPos2)
{
	return
		sqrt(
			((tempPos1->x - tempPos2->x) * (tempPos1->x - tempPos2->x)) +
			((tempPos1->y - tempPos2->y) * (tempPos1->y - tempPos2->y)) +
			((tempPos1->z - tempPos2->z) * (tempPos1->z - tempPos2->z))
		);
}
class SpringPoint
{
public:
	SpringPoint(NodeData& input_A, NodeData& input_B, float input_Bleed);

	NodeData* Parents[2];
	float push = 0;
	float pull = 0;
	float m_InertialLength;
	float SpringCoefficient;
	//float resistance
	//float memory
	CVector3 calculateForce(NodeData* particle);

	void updateCoefficient(float input, bool multiplier = false)
	{
		if (multiplier)
		{
			SpringCoefficient *= input;
		}
		else
		{
			SpringCoefficient = input;
		}
	}

	//Note: Likely need to store original places in order to calculate volume, i.e. It has moved X much so increase outwards-push by X.
	~SpringPoint()
	{
		delete this;
	}

	int getBoundParentCount()
	{
		int boundParentCount = 0;

		if (Parents[0]->isBound == true)
		{
			++boundParentCount;
		}

		if (Parents[1]->isBound == true)
		{
			++boundParentCount;
		}
		
		return boundParentCount;
	}
private:
};



