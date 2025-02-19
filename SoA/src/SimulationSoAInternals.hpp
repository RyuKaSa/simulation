#ifndef SIMULATION_SOA_INTERNALS_HPP
#define SIMULATION_SOA_INTERNALS_HPP

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp" // for ParticleType
#include <cstdlib>
#include <new>

// For dvec3, we want 32-byte alignment to be safe on typical 64-bit platforms.
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
    // Change glm::vec3 to glm::dvec3
    // And set alignment from 16 => 32 for double-based vectors.
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> position;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> velocity;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> forceAccum;
    std::vector<double> mass;
    std::vector<ParticleType> type;
    std::vector<bool> isStatic;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> color;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> dimensions;
};

// Now also double-based spring parameters:
struct SpringData {
    int p1Index;
    int p2Index;
    double restLength;
    double springConstant;
    double damping;
};

#endif // SIMULATION_SOA_INTERNALS_HPP