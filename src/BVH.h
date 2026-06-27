#pragma once
#include <glm/glm.hpp>
#include <vector>

// ─── AABB ─────────────────────────────────────────────────────────────────────
struct AABB {
    glm::vec3 min{ 1e30f };
    glm::vec3 max{ -1e30f };

    void expand(const glm::vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    glm::vec3 centroid() const { return (min + max) * 0.5f; }
    glm::vec3 extent()   const { return max - min; }

    bool intersectRay(const glm::vec3& origin,
        const glm::vec3& invDir,
        float tMin, float tMax,
        float& tHit) const;
};

// ─── BVH Node ─────────────────────────────────────────────────────────────────
struct BVHNode {
    AABB     bounds;
    int      left = -1;  // child index (-1 if leaf)
    int      right = -1;
    int      objectID = -1;  // if leaf
    bool     isLeaf()  const { return left == -1; }
};

// ─── Object (what BVH stores) ─────────────────────────────────────────────────
struct BVHObject {
    AABB      bounds;
    glm::vec3 centroid;
    int       id;
};

// ─── BVH ──────────────────────────────────────────────────────────────────────
class BVH {
public:
    void build(std::vector<BVHObject>& objects);
    void refit(std::vector<BVHObject>& objects);   // O(n) — called every frame

    struct HitResult {
        bool  hit = false;
        float t = 1e30f;
        int   objectID = -1;
    };

    HitResult raycast(const glm::vec3& origin,
        const glm::vec3& direction,
        float tMin = 0.001f,
        float tMax = 1e30f) const;

    void querySphere(const glm::vec3& center,
        float radius,
        std::vector<int>& outIDs) const;

    const std::vector<BVHNode>& nodes() const { return m_nodes; }
    int rootIndex() const { return m_root; }

private:
    std::vector<BVHNode> m_nodes;
    int                  m_root = -1;

    int buildRecursive(std::vector<BVHObject>& objects,
        int start, int end);
    void refitRecursive(int nodeIdx,
        std::vector<BVHObject>& objects);
    void queryRayRecursive(int nodeIdx,
        const glm::vec3& origin,
        const glm::vec3& invDir,
        float tMin, float tMax,
        HitResult& best) const;
    void querySphereRecursive(int nodeIdx,
        const glm::vec3& center,
        float radius,
        std::vector<int>& out) const;
};