#include "Entity.h"

Entity::Entity() {
    displayNameBuf[0] = '\0';
}

Entity::~Entity() {
    for (auto* c : m_components) {
        delete c;
    }
    m_components.clear();
}


