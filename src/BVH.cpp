#include "BVH.h"
#include <algorithm>
#include <cassert>

// ─── AABB ─────────────────────────────────────────────────────────────────────
bool AABB::intersectRay(const glm::vec3& origin,
    const glm::vec3& invDir,
    float tMin, float tMax,
    float& tHit) const  // ← add tHit output
{
    for (int i = 0; i < 3; i++) {
        float t0 = (min[i] - origin[i]) * invDir[i];
        float t1 = (max[i] - origin[i]) * invDir[i];
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMax < tMin) return false;
    }
    tHit = tMin;  // ← actual hit distance
    return true;
}

// ─── BVH Build ────────────────────────────────────────────────────────────────
void BVH::build(std::vector<BVHObject>& objects) {
    m_nodes.clear();
    if (objects.empty()) return;
    m_root = buildRecursive(objects, 0, (int)objects.size());
}

int BVH::buildRecursive(std::vector<BVHObject>& objects, int start, int end) {
    int nodeIdx = (int)m_nodes.size();
    m_nodes.push_back({});

    // Compute bounds
    AABB bounds;
    for (int i = start; i < end; i++)
        bounds.expand(objects[i].bounds);
    m_nodes[nodeIdx].bounds = bounds;

    int count = end - start;

    // Leaf
    if (count == 1) {
        m_nodes[nodeIdx].objectID = objects[start].id;
        return nodeIdx;
    }

    // Find longest axis
    glm::vec3 ext = bounds.extent();
    int axis = 0;
    if (ext.y > ext.x) axis = 1;
    if (ext.z > ext[axis]) axis = 2;

    float mid = bounds.centroid()[axis];

    auto it = std::partition(
        objects.begin() + start,
        objects.begin() + end,
        [axis, mid](const BVHObject& o) {
            return o.centroid[axis] < mid;
        });

    int splitIdx = (int)(it - objects.begin());
    if (splitIdx == start || splitIdx == end)
        splitIdx = start + count / 2;

    int leftIdx = buildRecursive(objects, start, splitIdx);
    int rightIdx = buildRecursive(objects, splitIdx, end);

    // Re-fetch after potential reallocation
    m_nodes[nodeIdx].left = leftIdx;
    m_nodes[nodeIdx].right = rightIdx;

    return nodeIdx;
}

// ─── BVH Refit (every frame) ──────────────────────────────────────────────────
void BVH::refit(std::vector<BVHObject>& objects) {
    if (m_root == -1) return;
    refitRecursive(m_root, objects);
}

void BVH::refitRecursive(int nodeIdx, std::vector<BVHObject>& objects) {
    BVHNode& node = m_nodes[nodeIdx];

    if (node.isLeaf()) {
        // Find the object and update bounds
        for (auto& obj : objects) {
            if (obj.id == node.objectID) {
                node.bounds = obj.bounds;
                break;
            }
        }
        return;
    }

    refitRecursive(node.left, objects);
    refitRecursive(node.right, objects);

    node.bounds = AABB{};
    node.bounds.expand(m_nodes[node.left].bounds);
    node.bounds.expand(m_nodes[node.right].bounds);
}

// ─── Raycast ──────────────────────────────────────────────────────────────────
BVH::HitResult BVH::raycast(const glm::vec3& origin,
    const glm::vec3& direction,
    float tMin, float tMax) const
{
    HitResult result;
    if (m_root == -1) return result;

    glm::vec3 invDir = 1.0f / direction;
    queryRayRecursive(m_root, origin, invDir, tMin, tMax, result);
    return result;
}

void BVH::queryRayRecursive(int nodeIdx,
    const glm::vec3& origin,
    const glm::vec3& invDir,
    float tMin, float tMax,
    HitResult& best) const
{
    const BVHNode& node = m_nodes[nodeIdx];

    float tHit;
    if (!node.bounds.intersectRay(origin, invDir, tMin, tMax, tHit))
        return;

    // Early exit — this node can't beat current best
    if (tHit >= best.t) return;

    if (node.isLeaf()) {
        best.hit = true;
        best.t = tHit;
        best.objectID = node.objectID;
        return;
    }

    queryRayRecursive(node.left, origin, invDir, tMin, tMax, best);
    queryRayRecursive(node.right, origin, invDir, tMin, tMax, best);
}
// ─── Sphere Query ─────────────────────────────────────────────────────────────
void BVH::querySphere(const glm::vec3& center,
    float radius,
    std::vector<int>& outIDs) const
{
    if (m_root == -1) return;
    querySphereRecursive(m_root, center, radius, outIDs);
}

void BVH::querySphereRecursive(int nodeIdx,
    const glm::vec3& center,
    float radius,
    std::vector<int>& out) const
{
    const BVHNode& node = m_nodes[nodeIdx];

    glm::vec3 closest = glm::clamp(center, node.bounds.min, node.bounds.max);
    float distSq = glm::dot(center - closest, center - closest);
    if (distSq > radius * radius) return;

    if (node.isLeaf()) {
        out.push_back(node.objectID);
        return;
    }

    querySphereRecursive(node.left, center, radius, out);
    querySphereRecursive(node.right, center, radius, out);
}