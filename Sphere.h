#pragma once

#include "Vec3.h"

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
public:
	Sphere(float x, float y, float z);
	~Sphere(void);

	void clearForce();
	void addGravityForce();
	void addForce(Vec3 force);

	Vec3 getPosition();

	void step1();
	void step2();
};

