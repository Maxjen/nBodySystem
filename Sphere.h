#pragma once

#include "Vec3.h"
#include <set>

using std::set;

class Sphere
{
private:
	Vec3 position;
	Vec3 velocity;
	Vec3 forceAccumulator;

	Vec3 positionTmp;
	Vec3 velocityTmp;

	float mass;
	float damping;

	float timeStep;
	set<int> collisionApplied;
public:
	Sphere(float x, float y, float z);
	~Sphere(void);

	void clearForce();
	void addGravityForce();
	void addForce(Vec3 force);
	void addForceFrom(Vec3 force, int i);

	Vec3 getPosition();

	void step1();
	void step2();
};

