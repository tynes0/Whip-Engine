#include "WhipPch.h"
#include "Whip/Physics/ContactFilter.h"

_WHIP_START

bool ContactFilter::ShouldCollide(b2ShapeId shapeA, b2ShapeId shapeB, void* context)
{
	(void)context;

	const b2Filter filterA = b2Shape_GetFilter(shapeA);
	const b2Filter filterB = b2Shape_GetFilter(shapeB);

	if (filterA.groupIndex == filterB.groupIndex && filterA.groupIndex != 0)
	{
		return filterA.groupIndex > 0;
	}

	bool collide = (filterA.maskBits & filterB.categoryBits) != 0 && (filterA.categoryBits & filterB.maskBits) != 0;
	return collide;
}

_WHIP_END
