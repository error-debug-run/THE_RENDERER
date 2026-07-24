# The Renderer

> **Date:** 17-07-2026

---

## Current Capabilities
After the renderer reaches a state where it can render the scene, it will be able to render the scene in real-time. The renderer will be able to handle complex scenes with multiple light sources, shadows, and reflections. It will also support various rendering techniques such as ray tracing, path tracing, and rasterization.

## The Challenge
But we came to a problem because, as per the renderer architecture which we had been using, it only supports pre-compiled shaders and does not support dynamic shader compilation. This means that any changes made to the shader code will require a recompilation of the entire shader program, which can be time-consuming and inefficient.

## The Solution
To address this issue, we have decided to implement a new renderer architecture that supports dynamic shader compilation. This will allow us to change the shaders and the scenes on demand and anytime, even while the renderer is running. And for it, we also got our scene...e file extension so that it helps the shader development to be eased out for the model and also us at any future point the documents to the shader can be found at the SCF-docs.md file.