#include "Sphere.h"


Sphere::Sphere(float x, float y, float z)
{
	position = Vec3(x, y, z);
	velocity.zero();
	forceAccumulator.zero();
	mass = 1.0f;
	damping = 0.1f;
	timeStep = 1.0f/60;
}

Sphere::~Sphere(void)
{
}

void Sphere::clearForce()
{
	forceAccumulator.zero();
}

void Sphere::addGravityForce()
{
	forceAccumulator += mass * Vec3(0, -10.0f, 0);
}

void Sphere::addForce(Vec3 force)
{
	forceAccumulator += force;
}

Vec3 Sphere::getPosition()
{
	return position;
}

void Sphere::step1()
{
	if (position.x <= -0.5f && velocity.x < 0)
		velocity.x *= -1;
	if (position.y <= -0.5f && velocity.y < 0)
		velocity.y *= -1;
	if (position.z <= -0.5f && velocity.z < 0)
		velocity.z *= -1;

	if (position.x >= 0.5f && velocity.x > 0)
		velocity.x *= -1;
	if (position.y >= 0.5f && velocity.y > 0)
		velocity.y *= -1;
	if (position.z >= 0.5f && velocity.z > 0)
		velocity.z *= -1;

	positionTmp = position + velocity * timeStep/2;
	if (positionTmp.x < -0.5f)
		positionTmp.x = -0.5f;
	if (positionTmp.y < -0.5f)
		positionTmp.y = -0.5f;
	if (positionTmp.z < -0.5f)
		positionTmp.z = -0.5f;

	if (positionTmp.x > 0.5f)
		positionTmp.x = 0.5f;
	if (positionTmp.y > 0.5f)
		positionTmp.y = 0.5f;
	if (positionTmp.z > 0.5f)
		positionTmp.z = 0.5f;

	velocityTmp = velocity + (forceAccumulator - (damping * velocity)) * (timeStep / 2) / mass;
	position += velocityTmp * timeStep;
	if (position.x < -0.5f)
		position.x = -0.5f;
	if (position.y < -0.5f)
		position.y = -0.5f;
	if (position.z < -0.5f)
		position.z = -0.5f;

	if (position.x > 0.5f)
		position.x = 0.5f;
	if (position.y > 0.5f)
		position.y = 0.5f;
	if (position.z > 0.5f)
		position.z = 0.5f;
}

void Sphere::step2()
{
	velocity += (forceAccumulator - (damping * velocityTmp)) * timeStep / mass;
}
