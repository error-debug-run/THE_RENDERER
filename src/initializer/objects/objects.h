// src/initializer/objects/Object.h
#pragma once
#include <string>

struct Object {
    std::string name = "";

    // physical
    float mass = 1.0f;
    float radius = 0.1f;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool  isStatic = false;

    // visual
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    std::string texture = "";
};