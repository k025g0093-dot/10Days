#include "MonoBehaviour.h"

MonoBehaviour::MonoBehaviour() : m_isStarted(false)
{
}

MonoBehaviour::~MonoBehaviour()
{
}

void MonoBehaviour::Start()
{
    m_isStarted = true;
}

void MonoBehaviour::Update()
{
    // Update logic here
}

void MonoBehaviour::FixedUpdate()
{
    // Fixed update logic here
}
