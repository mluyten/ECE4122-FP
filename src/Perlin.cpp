/*
Author: Matthew Luyten
Class: ECE6122
Last Date Modified: 11/16/2024

Description: 
*/

#define _USE_MATH_DEFINES

#include "Perlin.hpp"
#include <stddef.h>
#include <cmath>

/** 
 * @author Matt Luyten
 * @brief GradientNoise default constructor. Initializes random gradient generator with "random" seed based
 * on system time
 */
GradientNoise::GradientNoise() : _gradient1(std::time(NULL)), _gradient2(std::time(NULL)) {}

// Initialize object with desired seed
GradientNoise::GradientNoise(uint32_t seed) : _gradient1(seed), _gradient2(seed) {}

GradientNoise::~GradientNoise() {}

uint32_t lfsr(uint32_t seed, size_t shifts) {
    for (size_t i = 0; i < shifts; i++) {
        seed |= seed == 0;   // if seed == 0, set seed = 1 instead
        seed ^= (seed & 0x0007ffff) << 13;
        seed ^= seed >> 17;
        seed ^= (seed & 0x07ffffff) << 5;
        seed = seed & 0xffffffff;
    }
    return seed;
}

// Initialize 2D gradient generator with desired seed
GradientNoise::Gradient2::Gradient2(uint32_t seed) {
    _seed = seed;
}

// Get gradient at integer position (x,y)
glm::vec2 GradientNoise::Gradient2::at(glm::vec2 position) {
    std::pair<int, int> pos = std::pair<int, int>(position.x, position.y);
    std::unique_lock<std::mutex> lock(_m);
    if (_gradients.count(pos) == 0) {
        _gradients.emplace(pos, generate(position.x, position.y));
    }
    return _gradients[pos];
}

glm::vec2 GradientNoise::Gradient2::at(int x, int y) {
    std::pair<int, int> pos = std::pair<int, int>(x, y);
    std::unique_lock<std::mutex> lock(_m);
    if (_gradients.count(pos) == 0) {
        _gradients.emplace(pos, generate(x, y));
    }
    return _gradients[pos];
}

glm::vec2 GradientNoise::Gradient2::generate(int x, int y) {
    int N = 8;
    uint32_t perm = lfsr(_seed + x, abs(x)); // Get a random number from 0 to 7
    perm = lfsr(perm + y, abs(y));
    perm = perm % N;

    // Return corresponding gradient
    if (perm == 0)
        return glm::vec2(1, 0);
    else if (perm == 1)
        return glm::vec2(-1, 0);
    else if (perm == 2)
        return glm::vec2(0, 1);
    else if (perm == 3)
        return glm::vec2(0, -1);
    else if (perm == 4)
        return glm::vec2(0.7071, 0.7071);
    else if (perm == 5)
        return glm::vec2(0.7071, -0.7071);
    else if (perm == 6)
        return glm::vec2(-0.7071, 0.7071);
    else
        return glm::vec2(-0.7071, -0.7071);
}

GradientNoise::Gradient1::Gradient1(uint32_t seed) {
    _seed = seed;
}

double GradientNoise::Gradient1::at(int x) {
    if (_gradients.count(x) == 0) {
        _gradients.emplace(x, generate(x));
    }
    return _gradients[x];
}

double GradientNoise::Gradient1::generate(int x) {
    int N = 5;
    uint32_t perm = lfsr(_seed + x, abs(x));
    perm = perm % N;
    if (perm == 0)
        return 1;
    else if (perm == 1)
        return 0.5;
    else if (perm == 2)
        return 0;
    else if (perm == 3)
        return -0.5;
    else
        return -1;
}

double GradientNoise::perlin1D(double x) {
    double p = x;
    double u = p - floor(p);
    double n0 = _gradient1.at(floor(p)) * u;
    double n1 = _gradient1.at(ceil(p)) * (u-1);
    double fade = easeCurve<double>(u);
    return n0 + fade * (n1-n0);
}

double GradientNoise::fractalPerlin1D(double x, int octaves, double freqStart, double freqRate, double ampRate) {
    double y = 0;
    double freq = freqStart;
    double amplitude = 1;
    for (int k = 0; k < octaves; k++) {
        y += amplitude * perlin1D(x*freq);
        amplitude *= ampRate;
        freq *= freqRate;
    }
    return y;
}

glm::vec3 GradientNoise::perlin2D(double x, double y) {
    double u = x - floor(x);
    double v = y - floor(y);
    double n00 = glm::dot(_gradient2.at(floor(x), floor(y)), glm::vec2(u, v));
    double n10 = glm::dot(_gradient2.at(floor(x)+1, floor(y)), glm::vec2(u-1, v));
    double n01 = glm::dot(_gradient2.at(floor(x), floor(y)+1), glm::vec2(u, v-1));
    double n11 = glm::dot(_gradient2.at(floor(x)+1, floor(y)+1), glm::vec2(u-1, v-1));
    double nx0 = n00 * (1 - easeCurve<double>(u)) + n10 * easeCurve<double>(u);
    double nx1 = n01 * (1 - easeCurve<double>(u)) + n11 * easeCurve<double>(u);
    return glm::vec3(easeCurveGradient<double>(u) * (n10 - n00 + (n00 - n10 - n01 + n11) * easeCurve<double>(v)),
            easeCurveGradient<double>(v) * (n00 - n01 + (n00 - n10 - n01 + n11) * easeCurve<double>(u)),
            nx0 * (1 - easeCurve<double>(v)) + nx1 * easeCurve<double>(v));
}

double GradientNoise::fractalPerlin2D(double x, double y, double max, int mode, int octaves, double freqStart, double freqRate, double ampRate) {
    double height = 0;
    double freq = freqStart;
    double amplitude = 1;
    if (mode == 1 || mode == 2)
        freq = freq / 2;
    
    for (int k = 0; k < octaves; k++) {
        glm::vec3 noise = perlin2D(x*freq, y*freq);
        if (k == 0 && mode == 3) amplitude / (sqrt(noise.x * noise.x + noise.y * noise.y) * 2 + 0.8);
        if (mode == 1 || mode == 2)  // Turbulent or opalescent
            height += amplitude * abs(noise.z);
        else // Standard fractal
            height += amplitude * noise.z;
        amplitude *= ampRate;
        freq *= freqRate;
    }
    if (mode == 1)
        height = height * max * 2 - max;
    else if (mode == 2)
        height = max * cos(2 * M_PI * height);
    else
        height *= max;
    return height;
}

void GradientNoise::fractalPerlin2D(glm::vec3& pos, double max, int mode, int octaves, double freqStart, double freqRate, double ampRate) {
    double height = 0;
    double freq = freqStart;
    double amplitude = 1;
    if (mode == 1 || mode == 2)
        freq = freq / 2;

    for (int k = 0; k < octaves; k++) {
        glm::vec3 noise = perlin2D(pos.x*freq, pos.z*freq);
        if (k == 0 && mode == 3) amplitude / (sqrt(noise.x * noise.x + noise.y * noise.y) * 2 + 0.8);
        
        if (mode == 1 || mode == 2) // Turbulent or opalescent
            pos.y += amplitude * abs(noise.z);
        else // Standard fractal
            pos.y += amplitude * noise.z;
        amplitude *= ampRate;
        freq *= freqRate;
    }
    if (mode == 1)
        pos.y = pos.y * 2 * max - max;
    else if (mode == 2)
        pos.y = max / 5 * cos(2 * M_PI * pos.y);
    else
        pos.y *= max;
    return;
}