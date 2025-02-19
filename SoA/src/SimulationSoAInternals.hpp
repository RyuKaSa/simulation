#ifndef SIMULATION_SOA_INTERNALS_HPP
#define SIMULATION_SOA_INTERNALS_HPP

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp" // for ParticleType
#include <cstdlib>
#include <new>

// Note: On Windows, you’d substitute with _aligned_malloc/_aligned_free.
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
bool operator==(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) { return true; }

template <typename T, typename U, std::size_t Alignment>
bool operator!=(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) { return false; }

struct ParticleSoA {
    std::vector<glm::vec3, AlignedAllocator<glm::vec3, 16>> position;
    std::vector<glm::vec3, AlignedAllocator<glm::vec3, 16>> velocity;
    std::vector<glm::vec3, AlignedAllocator<glm::vec3, 16>> forceAccum; 
    std::vector<float> mass;
    std::vector<ParticleType> type;
    std::vector<bool> isStatic;
    std::vector<glm::vec3, AlignedAllocator<glm::vec3, 16>> color;
    std::vector<glm::vec3, AlignedAllocator<glm::vec3, 16>> dimensions;
};

struct SpringData {
    int p1Index;
    int p2Index;
    float restLength;
    float springConstant;
    float damping;
};

#endif // SIMULATION_SOA_INTERNALS_HPP