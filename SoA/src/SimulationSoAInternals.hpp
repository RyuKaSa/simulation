#ifndef SIMULATION_SOA_INTERNALS_HPP
#define SIMULATION_SOA_INTERNALS_HPP

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp" // for ParticleType
#include <cstdlib>
#include <new>

// Define the custom aligned allocator first.
template <typename T, std::size_t Alignment>
struct AlignedAllocator {
    using value_type = T;
    AlignedAllocator() noexcept {}
    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}
    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
    T* allocate(std::size_t n) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0)
            throw std::bad_alloc();
        return reinterpret_cast<T*>(ptr);
    }
    void deallocate(T* p, std::size_t) noexcept {
        free(p);
    }
};

template <typename T, typename U, std::size_t Alignment>
bool operator==(const AlignedAllocator<T, Alignment>&,
                const AlignedAllocator<U, Alignment>&) { return true; }

template <typename T, typename U, std::size_t Alignment>
bool operator!=(const AlignedAllocator<T, Alignment>&,
                const AlignedAllocator<U, Alignment>&) { return false; }

// Now using double-based vectors:
struct ParticleSoA {
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> position;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> velocity;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> forceAccum;
    std::vector<double> mass;
    std::vector<ParticleType> type;
    std::vector<bool> isStatic;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> color;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> dimensions;
    std::vector<int> clothID;
};

struct SpringData {
    int p1Index;
    int p2Index;
    double restLength;
    double springConstant;
    double damping;
};

// Mark these functions inline to avoid duplicate symbols
inline void reorderSoAAndFixSprings(
    ParticleSoA &soA,
    std::vector<SpringData> &springs,
    const std::vector<size_t> &newOrder)
{
    const size_t total = soA.position.size();
    if (newOrder.size() != total) {
        throw std::runtime_error("newOrder size does not match SoA size!");
    }
    
    std::vector<size_t> oldToNew(total);
    for (size_t newIdx = 0; newIdx < total; newIdx++) {
        size_t oldIdx = newOrder[newIdx];
        if (oldIdx >= total) {
            throw std::runtime_error("Invalid oldIdx in newOrder!");
        }
        oldToNew[oldIdx] = newIdx;
    }
    
    auto oldPosition    = soA.position;
    auto oldVelocity    = soA.velocity;
    auto oldForceAccum  = soA.forceAccum;
    auto oldColor       = soA.color;
    auto oldDimensions  = soA.dimensions;
    auto oldMass        = soA.mass;
    auto oldType        = soA.type;
    auto oldIsStatic    = soA.isStatic;
    auto oldClothID     = soA.clothID;
    
    for (size_t newIdx = 0; newIdx < total; newIdx++) {
        size_t oldIdx = newOrder[newIdx];
        soA.position   [newIdx] = oldPosition   [oldIdx];
        soA.velocity   [newIdx] = oldVelocity   [oldIdx];
        soA.forceAccum [newIdx] = oldForceAccum [oldIdx];
        soA.color      [newIdx] = oldColor      [oldIdx];
        soA.dimensions [newIdx] = oldDimensions [oldIdx];
        soA.mass       [newIdx] = oldMass       [oldIdx];
        soA.type       [newIdx] = oldType       [oldIdx];
        soA.isStatic   [newIdx] = oldIsStatic   [oldIdx];
        soA.clothID    [newIdx] = oldClothID    [oldIdx];
    }
    
    for (auto &sp : springs) {
        sp.p1Index = static_cast<int>(oldToNew[sp.p1Index]);
        sp.p2Index = static_cast<int>(oldToNew[sp.p2Index]);
    }
}

inline void reorderClothFirst(ParticleSoA &soA, std::vector<SpringData> &springs)
{
    const size_t n = soA.position.size();
    std::vector<size_t> clothIndices;
    clothIndices.reserve(n);
    std::vector<size_t> externalIndices;
    externalIndices.reserve(n);
    
    for (size_t i = 0; i < n; i++) {
        if (soA.clothID[i] >= 0) {
            clothIndices.push_back(i);
        }
    }
    for (size_t i = 0; i < n; i++) {
        if (soA.clothID[i] < 0) {
            externalIndices.push_back(i);
        }
    }
    
    std::vector<size_t> newOrder;
    newOrder.reserve(n);
    newOrder.insert(newOrder.end(), clothIndices.begin(), clothIndices.end());
    newOrder.insert(newOrder.end(), externalIndices.begin(), externalIndices.end());
    
    reorderSoAAndFixSprings(soA, springs, newOrder);
}

// Removes the particle at removeIndex from soA and updates springs accordingly.
// After reordering, the vectors are resized to the new size.
inline void removeParticle(ParticleSoA &soA, std::vector<SpringData> &springs, size_t removeIndex) {
    size_t n = soA.position.size();
    if (removeIndex >= n) return; // Nothing to do.

    // Build a new ordering that skips the removed index.
    std::vector<size_t> newOrder;
    newOrder.reserve(n - 1);
    for (size_t i = 0; i < n; i++) {
        if (i == removeIndex)
            continue;
        newOrder.push_back(i);
    }
    
    // Reorder SoA and fix spring indices accordingly.
    reorderSoAAndFixSprings(soA, springs, newOrder);
    
    // Resize each vector to remove the last (now unused) element.
    soA.position.resize(n - 1);
    soA.velocity.resize(n - 1);
    soA.forceAccum.resize(n - 1);
    soA.mass.resize(n - 1);
    soA.type.resize(n - 1);
    soA.isStatic.resize(n - 1);
    soA.color.resize(n - 1);
    soA.dimensions.resize(n - 1);
    soA.clothID.resize(n - 1);
}

#endif // SIMULATION_SOA_INTERNALS_HPP