#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"
#include "Entity.h"
#include "Framebuffer.h"

#ifndef ASSET_DIR
#define ASSET_DIR "assets/"   // fallback if the build system didn't set it
#endif

// Resolution of the shadow map (the depth texture rendered from the light).
// Bigger = sharper shadows but more memory / fill cost. 2048 is a good default.
const unsigned int SHADOW_WIDTH  = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

// =====================================================================
//  SHADERS
// =====================================================================

// ---- PASS 2 vertex shader: the normal scene shader ----
// New vs. before: it also outputs the fragment's position as seen FROM THE
// LIGHT (FragPosLightSpace). That coordinate is what we use to look the pixel
// up in the shadow map.
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;   // lightProjection * lightView

out vec3 FragPos;            // world-space position
out vec3 Normal;             // world-space normal
out vec2 TexCoord;
out vec4 FragPosLightSpace;  // this vertex as seen from the light

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    gl_Position = uProjection * uView * worldPos;

    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord;

    // Same vertex, but projected through the light's camera. We will divide by w
    // and remap to [0,1] in the fragment shader to index the shadow map.
    FragPosLightSpace = uLightSpaceMatrix * worldPos;
}
)";

// ---- PASS 2 fragment shader: lighting + shadow lookup ----
const char* fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform sampler2D uShadowMap;   // the depth texture rendered from the light
uniform vec3 uViewPos;

// The three coloured point lights are "fill" light and DO NOT cast shadows.
#define NUM_LIGHTS 3
struct Light {
    vec3 position;
    vec3 color;
};
uniform Light uLights[NUM_LIGHTS];

// A single directional "sun". This is the ONLY light that casts shadows,
// because the shadow map was rendered from its point of view.
uniform vec3 uSunDir;     // direction pointing FROM the surface TOWARD the sun
uniform vec3 uSunColor;

uniform vec3  uAmbientColor;
uniform float uShininess;
uniform float uSpecularStrength;

// Returns 1.0 if this pixel is in shadow (blocked from the sun), 0.0 if lit.
// 'bias' nudges the comparison to fight "shadow acne" (see walkthrough).
float ShadowCalculation(vec4 fragPosLightSpace, float bias) {
    // 1) Perspective divide. For an orthographic light this is a no-op (w==1),
    //    but doing it always keeps the code correct for any projection.
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // 2) The depth texture stores values in [0,1], but proj is in NDC [-1,1].
    //    Remap so proj.xy are texture coords and proj.z is a comparable depth.
    proj = proj * 0.5 + 0.5;

    // 3) Anything beyond the light's far plane has no depth info -> treat as lit.
    if (proj.z > 1.0)
        return 0.0;

    // 4) How far this pixel is from the light (its own depth).
    float currentDepth = proj.z;

    // 5) PCF (Percentage-Closer Filtering): instead of one hard yes/no test,
    //    sample a 3x3 block of neighbouring shadow-map texels and average the
    //    results. This softens the otherwise jagged, pixelated shadow edge.
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(uShadowMap, proj.xy + vec2(x, y) * texelSize).r;
            // If our pixel is further from the light than the nearest blocker
            // recorded in the map, something is between us and the sun.
            shadow += (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;   // average of the 9 samples

    return shadow;
}

void main() {
    // Base colour, sRGB -> linear (lighting math wants linear light).
    vec3 base = texture(uTexture, TexCoord).rgb;
    base = pow(base, vec3(2.2));

    vec3 N = normalize(Normal);
    vec3 V = normalize(uViewPos - FragPos);

    // Ambient fill so nothing is pure black.
    vec3 result = uAmbientColor * base;

    // --- the three point lights: unshadowed fill light ---
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        vec3  toLight = uLights[i].position - FragPos;
        vec3  L       = normalize(toLight);
        float dist    = length(toLight);

        float diff    = max(dot(N, L), 0.0);
        vec3  diffuse = diff * uLights[i].color;

        vec3  H       = normalize(L + V);
        float spec    = pow(max(dot(N, H), 0.0), uShininess);
        vec3  specular= uSpecularStrength * spec * uLights[i].color;

        float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        result += (diffuse * base + specular) * atten;
    }

    // --- the sun: directional light that DOES cast shadows ---
    vec3  L    = normalize(uSunDir);
    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);

    // Slope-scaled bias: surfaces nearly edge-on to the sun (small dot(N,L))
    // need a bigger nudge to avoid self-shadowing speckle.
    float bias   = max(0.05 * (1.0 - dot(N, L)), 0.005);
    float shadow = ShadowCalculation(FragPosLightSpace, bias);

    vec3 sun = (diff * base + uSpecularStrength * spec) * uSunColor;
    result += (1.0 - shadow) * sun;   // shadow removes the sun's contribution only

    // Linear -> sRGB for the monitor.
    result = pow(result, vec3(1.0 / 2.2));
    FragColor = vec4(result, 1.0);
}
)";

// ---- PASS 1 shaders: render depth from the light's point of view ----
// The vertex shader transforms geometry into the light's clip space. The
// fragment shader does nothing — the GPU writes depth into the bound depth
// texture automatically. We just need a (valid, empty) fragment stage.
const char* depthVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
)";

const char* depthFragmentSource = R"(
#version 330 core
void main() {
    // intentionally empty: depth is written for us
}
)";

// ---- PASS 3 shaders: post-processing on a full-screen quad ----
// The scene has already been rendered into an offscreen texture. Now we draw a
// single quad that covers the whole screen and, for every pixel, look up the
// matching pixel of that texture and (optionally) transform it. This is where
// screen-space effects live: grayscale, colour grading, blur, bloom, FXAA, and
// the upscaler this is groundwork for.
//
// The vertex shader is trivial: the quad's positions are ALREADY in clip space
// (-1..1), so we pass them straight through. No model/view/projection at all.
const char* postVertexSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;       // already in NDC, -1..1
layout (location = 1) in vec2 aTexCoord;  // 0..1 across the screen
out vec2 TexCoord;
void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* postFragmentSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uScene;   // the offscreen texture the scene was rendered into
uniform int uEffect;        // 0 none, 1 grayscale, 2 invert, 3 vignette

void main() {
    vec3 c = texture(uScene, TexCoord).rgb;

    if (uEffect == 1) {
        // Perceptual luminance: eyes are most sensitive to green, least to blue.
        float g = dot(c, vec3(0.299, 0.587, 0.114));
        c = vec3(g);
    } else if (uEffect == 2) {
        c = 1.0 - c;            // photo-negative
    } else if (uEffect == 3) {
        // Vignette: darken toward the corners. distance from screen centre.
        vec2  d = TexCoord - 0.5;
        float v = smoothstep(0.75, 0.2, length(d));   // 1 at centre -> 0 at edge
        c *= v;
    }
    // uEffect == 0 falls through unchanged: a plain copy of the scene.

    FragColor = vec4(c, 1.0);
}
)";

// =====================================================================
//  INPUT / CAMERA (unchanged)
// =====================================================================

float lastX = 400.0f, lastY = 300.0f;
bool  firstMouse = true;
float yaw   = -90.0f;
float pitch = 0.0f;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile failed:\n" << log << '\n';
    }
    return shader;
}

// Compile a vertex+fragment pair into a linked program. We now build two
// programs (scene + depth), so this removes the duplicated boilerplate.
unsigned int makeProgram(const char* vs, const char* fs) {
    unsigned int v = compileShader(GL_VERTEX_SHADER,   vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    unsigned int program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link failed:\n" << log << '\n';
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return program;
}

void user_input(GLFWwindow* window, Camera* Cam, float DeltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) Cam->move_forward(DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) Cam->move_backward(DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) Cam->move_left(DeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) Cam->move_right(DeltaTime);
    Cam->set_direction(yaw, pitch);
}

// Build a flat square on the ground (the XZ plane, at y = 0) so there is a
// surface for the cube's shadow to fall on. 'half' is the half-width; 'uvTile'
// controls how many times the texture repeats across it.
std::vector<Vertex> makeFloor(float half, float uvTile) {
    glm::vec3 n(0.0f, 1.0f, 0.0f);   // faces straight up
    float u = uvTile;
    return {
        {{-half, 0.0f, -half}, n, {0.0f, 0.0f}},
        {{ half, 0.0f, -half}, n, {u,    0.0f}},
        {{ half, 0.0f,  half}, n, {u,    u   }},

        {{-half, 0.0f, -half}, n, {0.0f, 0.0f}},
        {{ half, 0.0f,  half}, n, {u,    u   }},
        {{-half, 0.0f,  half}, n, {0.0f, u   }},
    };
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Renderer", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glEnable(GL_DEPTH_TEST);

    // Three programs now: the lit scene shader, the depth-only shader, and the
    // post-processing shader that runs over the offscreen image.
    unsigned int shaderProgram = makeProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int depthShader   = makeProgram(depthVertexSource,  depthFragmentSource);
    unsigned int postShader    = makeProgram(postVertexSource,   postFragmentSource);

    {
    Mesh cubeMesh(loadOBJ(ASSET_DIR "cube.obj"));
    Mesh floorMesh(makeFloor(10.0f, 8.0f));
    Texture cubeTexture(ASSET_DIR "test.png");

    // ---- scene-shader uniform locations ----
    int modelLoc      = glGetUniformLocation(shaderProgram, "uModel");
    int viewLoc       = glGetUniformLocation(shaderProgram, "uView");
    int projectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
    int textureLoc    = glGetUniformLocation(shaderProgram, "uTexture");

    int viewPosLoc          = glGetUniformLocation(shaderProgram, "uViewPos");
    int ambientLoc          = glGetUniformLocation(shaderProgram, "uAmbientColor");
    int shininessLoc        = glGetUniformLocation(shaderProgram, "uShininess");
    int specularStrengthLoc = glGetUniformLocation(shaderProgram, "uSpecularStrength");

    int lightSpaceLoc = glGetUniformLocation(shaderProgram, "uLightSpaceMatrix");
    int shadowMapLoc  = glGetUniformLocation(shaderProgram, "uShadowMap");
    int sunDirLoc     = glGetUniformLocation(shaderProgram, "uSunDir");
    int sunColorLoc   = glGetUniformLocation(shaderProgram, "uSunColor");

    // ---- depth-shader uniform locations ----
    int depthLightSpaceLoc = glGetUniformLocation(depthShader, "uLightSpaceMatrix");
    int depthModelLoc      = glGetUniformLocation(depthShader, "uModel");

    // ---- post-shader uniform locations ----
    int postSceneLoc  = glGetUniformLocation(postShader, "uScene");
    int postEffectLoc = glGetUniformLocation(postShader, "uEffect");

    // =====================================================================
    //  SCREEN QUAD: two triangles covering the whole screen in clip space
    // =====================================================================
    // Each vertex is just 4 floats: x, y (already in NDC -1..1) and u, v (the
    // texture coordinate 0..1). No 3D, no Mesh class -- this geometry never
    // moves, so a tiny hand-written buffer is simplest.
    float quadVertices[] = {
        // positions   // texcoords
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,

        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // attribute 0 = position : 2 floats at the start of each vertex.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // attribute 1 = texcoord : 2 floats right after the position.
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // =====================================================================
    //  OFFSCREEN TARGET: the scene is rendered into this instead of the window
    // =====================================================================
    int initW, initH;
    glfwGetFramebufferSize(window, &initW, &initH);
    Framebuffer offscreen(initW, initH);

    // Which post effect is active (toggle with number keys 0-3 in the loop).
    int postEffect = 0;

    // The three coloured point lights (dimmed a touch so the sun's shadow reads
    // clearly on the floor).
    glm::vec3 lightPositions[3] = {
        glm::vec3( 2.0f,  2.0f,  2.0f),
        glm::vec3(-2.0f,  1.0f, -1.5f),
        glm::vec3( 0.0f,  2.0f,  2.0f),
    };
    glm::vec3 lightColors[3] = {
        glm::vec3(0.5f, 0.45f, 0.4f),
        glm::vec3(0.15f, 0.2f, 0.5f),
        glm::vec3(0.5f, 0.15f, 0.15f),
    };

    int lightPosLoc[3];
    int lightColorLoc[3];
    for (int i = 0; i < 3; ++i) {
        std::string base = "uLights[" + std::to_string(i) + "]";
        lightPosLoc[i]   = glGetUniformLocation(shaderProgram, (base + ".position").c_str());
        lightColorLoc[i] = glGetUniformLocation(shaderProgram, (base + ".color").c_str());
    }

    // =====================================================================
    //  SHADOW MAP: a framebuffer whose only attachment is a depth texture
    // =====================================================================
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    // A depth-only texture: one channel storing distance-from-light per texel.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Outside the light's frustum we want "fully lit", not a stretched edge
    // texel. CLAMP_TO_BORDER + a white (1.0 = farthest) border means any
    // out-of-range lookup reads max depth, so nothing is falsely shadowed.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthMap, 0);
    // This FBO has no colour attachment. We must tell OpenGL we won't draw or
    // read colour, or the framebuffer is considered incomplete.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Shadow framebuffer is not complete!\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // =====================================================================
    //  THE SUN: where the shadow-casting light lives, and its light-space matrix
    // =====================================================================
    // The sun is directional, so only its DIRECTION matters for shading. For
    // the shadow pass we still need a position to "stand" at and an orthographic
    // box that contains the scene.
    glm::vec3 lightPos = glm::vec3(-2.0f, 4.0f, -1.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // Orthographic (parallel) projection: a directional light has no perspective.
    glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 20.0f);
    glm::mat4 lightSpaceMatrix = lightProj * lightView;

    // Direction FROM a surface TOWARD the sun (constant for a directional light).
    glm::vec3 sunDir   = glm::normalize(lightPos);
    glm::vec3 sunColor = glm::vec3(1.0f, 0.95f, 0.9f);

    // =====================================================================
    //  THE SCENE: a flat list of entities (geometry + transform + animation)
    // =====================================================================
    // Everything we draw now lives in this vector. Adding an object to the world
    // is just push_back; the render loop and BOTH passes pick it up for free.
    std::vector<Entity> entities;

    // The floor: static, sitting one unit below the origin. No update -> it
    // never moves. (Its transform.scale stays 1 and rotation stays 0.)
    Entity floor;
    floor.mesh = &floorMesh;
    floor.transform.position = glm::vec3(0.0f, -1.0f, 0.0f);
    entities.push_back(floor);

    // A cube spinning in place at the centre, lifted to y=0.5 to clear the floor.
    // Its update() rewrites the rotation from the clock every frame.
    Entity spinner;
    spinner.mesh = &cubeMesh;
    spinner.transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    spinner.update = [](Transform& t, float time) {
        t.rotation = glm::vec3(time * 25.0f, time * 50.0f, 0.0f);   // degrees/sec
    };
    entities.push_back(spinner);

    // A smaller cube that ORBITS the spinner and BOBS up and down -- this shows
    // the transform's position (not just rotation) being animated over time.
    Entity orbiter;
    orbiter.mesh = &cubeMesh;
    orbiter.transform.scale = glm::vec3(0.5f);
    orbiter.update = [](Transform& t, float time) {
        float radius = 3.0f;
        t.position = glm::vec3(std::cos(time) * radius,
                               0.5f + std::sin(time * 2.0f) * 0.4f,   // bob
                               std::sin(time) * radius);
        t.rotation = glm::vec3(0.0f, time * 90.0f, 0.0f);            // also spins
    };
    entities.push_back(orbiter);

    // Draw every entity with whatever program is bound. We pass the program's
    // uModel location so the same code serves both the depth and the scene pass.
    auto renderScene = [&](int mLoc) {
        for (const Entity& e : entities) {
            glm::mat4 model = e.transform.matrix();
            glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(model));
            e.mesh->draw();
        }
    };

    auto Cam = Camera();
    auto view = Cam.get_lookat();

    float previousTime = 0.0f, nowTime = 0.0f, DeltaTime = 0.0f;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    while (!glfwWindowShouldClose(window)) {
        previousTime = nowTime;
        nowTime = glfwGetTime();
        DeltaTime = nowTime - previousTime;

        processInput(window);
        user_input(window, &Cam, DeltaTime);
        view = Cam.get_lookat();

        // Pick a post-processing effect with the number keys.
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) postEffect = 0; // none
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) postEffect = 1; // grayscale
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) postEffect = 2; // invert
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) postEffect = 3; // vignette

        // ANIMATION STEP: let every entity update its own transform from the
        // clock before we draw anything. Static entities (empty update) skip it.
        float time = (float)glfwGetTime();
        for (Entity& e : entities)
            if (e.update) e.update(e.transform, time);

        // ---------------------------------------------------------------
        //  PASS 1: render the scene's depth from the sun into depthMap
        // ---------------------------------------------------------------
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(depthShader);
        glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        renderScene(depthModelLoc);

        // ---------------------------------------------------------------
        //  PASS 2: render the scene into the OFFSCREEN texture (not the window),
        //          sampling depthMap for shadows exactly as before
        // ---------------------------------------------------------------
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        // Keep the offscreen image the same size as the window so it stays sharp
        // when the window is resized. (No-op on frames where nothing changed.)
        offscreen.resize(fbW, fbH);

        offscreen.bind();                       // draw into our texture, not the window
        glViewport(0, 0, fbW, fbH);
        glEnable(GL_DEPTH_TEST);                // the 3D scene needs depth testing
        glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f), (float)fbW / (float)fbH, 0.1f, 100.0f);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc,       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glUniform3fv(viewPosLoc, 1, glm::value_ptr(Cam.eye));
        glUniform3f(ambientLoc, 0.08f, 0.08f, 0.08f);
        glUniform1f(shininessLoc, 64.0f);
        glUniform1f(specularStrengthLoc, 0.5f);

        glUniform3fv(sunDirLoc,   1, glm::value_ptr(sunDir));
        glUniform3fv(sunColorLoc, 1, glm::value_ptr(sunColor));

        for (int i = 0; i < 3; ++i) {
            glUniform3fv(lightPosLoc[i],   1, glm::value_ptr(lightPositions[i]));
            glUniform3fv(lightColorLoc[i], 1, glm::value_ptr(lightColors[i]));
        }

        // Texture unit 0 = the colour texture; unit 1 = the shadow map.
        cubeTexture.bind(0);
        glUniform1i(textureLoc, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glUniform1i(shadowMapLoc, 1);

        renderScene(modelLoc);

        // ---------------------------------------------------------------
        //  PASS 3: draw the offscreen texture onto the window through the
        //          post-processing shader (a single full-screen quad)
        // ---------------------------------------------------------------
        Framebuffer::unbind();                  // back to the window (framebuffer 0)
        glViewport(0, 0, fbW, fbH);
        // The quad fills the screen, so there is nothing to depth-test against and
        // no 3D depth to keep. Turning depth testing off guarantees it always draws.
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(postShader);
        glUniform1i(postEffectLoc, postEffect);
        // Bind the scene texture on unit 0 and point the sampler at it.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, offscreen.colorTexture());
        glUniform1i(postSceneLoc, 0);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);       // 2 triangles = 6 vertices
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMap);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    }   // meshes / texture / offscreen go out of scope here -> GPU buffers freed

    glDeleteProgram(shaderProgram);
    glDeleteProgram(depthShader);
    glDeleteProgram(postShader);
    glfwTerminate();
    return 0;
}
