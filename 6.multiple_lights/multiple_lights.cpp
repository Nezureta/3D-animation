#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <string>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
void generateSphere(std::vector<float>& vertices,
                    std::vector<unsigned int>& indices,
                    float radius,
                    unsigned int sectorCount,
                    unsigned int stackCount);
void generateRing(std::vector<float>& vertices,
                  std::vector<unsigned int>& indices,
                  float innerRadius,
                  float outerRadius,
                  unsigned int sectorCount);

// One body in the solar system. Negative spinPerDay means retrograde rotation.
struct Planet {
    const char*  name;
    unsigned int texture;
    float        orbitRadius;   // distance from sun (world units)
    float        scale;         // sphere radius in world units
    float        orbitPerDay;   // radians of orbital motion per simulated day
    float        spinPerDay;    // radians of axial spin per simulated day (sign = direction)
};

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera (pulled back so all eight orbits fit on screen at startup)
Camera camera(glm::vec3(0.0f, 22.0f, 65.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Sun Earth Moon", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader programs
    // -------------------------------------
    Shader lightingShader("6.multiple_lights.vs", "6.multiple_lights.fs");
    Shader lightCubeShader("6.light_cube.vs", "6.light_cube.fs");
    Shader ringShader     ("6.ring.vs",          "6.ring.fs");

    // generate sphere geometry (unit sphere; we will scale per body)
    // --------------------------------------------------------------
    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    generateSphere(sphereVertices, sphereIndices, 1.0f, 48, 24);
    const GLsizei sphereIndexCount = static_cast<GLsizei>(sphereIndices.size());

    // configure the sphere VAO/VBO/EBO
    // --------------------------------
    unsigned int VBO, EBO, sphereVAO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(sphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sphereVertices.size() * sizeof(float),
                 sphereVertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sphereIndices.size() * sizeof(unsigned int),
                 sphereIndices.data(),
                 GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // generate Saturn-ring geometry (a flat annulus). Inner/outer radii are
    // expressed in world units; saturnFrame later places it at Saturn's location.
    // Saturn's planet scale is 1.20, real ring inner/outer ≈ 1.4 / 2.3 Saturn radii.
    // ---------------------------------------------------------------------------
    const float SATURN_SCALE       = 1.20f;
    const float saturnRingInner    = SATURN_SCALE * 1.40f;
    const float saturnRingOuter    = SATURN_SCALE * 2.30f;

    std::vector<float> ringVertices;
    std::vector<unsigned int> ringIndices;
    generateRing(ringVertices, ringIndices, saturnRingInner, saturnRingOuter, 128);
    const GLsizei ringIndexCount = static_cast<GLsizei>(ringIndices.size());

    unsigned int ringVAO, ringVBO, ringEBO;
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    glGenBuffers(1, &ringEBO);

    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 ringVertices.size() * sizeof(float),
                 ringVertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ringEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 ringIndices.size() * sizeof(unsigned int),
                 ringIndices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // load textures (we now use a utility function to keep the code more organized)
    // -----------------------------------------------------------------------------
    // Per-body diffuse maps from Solar System Scope (CC-BY 4.0). The sun is
    // emissive; the moon and the eight planets share a generic specular map.
    unsigned int sunTexture     = loadTexture(FileSystem::getPath("resources/textures/planets/2k_sun.png").c_str());
    unsigned int mercuryTexture = loadTexture(FileSystem::getPath("resources/textures/planets/2k_mercury.png").c_str());
    unsigned int venusTexture   = loadTexture(FileSystem::getPath("resources/textures/planets/2k_venus_surface.png").c_str());
    unsigned int earthTexture   = loadTexture(FileSystem::getPath("resources/textures/planets/2k_earth_daymap.png").c_str());
    unsigned int marsTexture    = loadTexture(FileSystem::getPath("resources/textures/planets/2k_mars.png").c_str());
    unsigned int jupiterTexture = loadTexture(FileSystem::getPath("resources/textures/planets/2k_jupiter.png").c_str());
    unsigned int saturnTexture  = loadTexture(FileSystem::getPath("resources/textures/planets/2k_saturn.png").c_str());
    unsigned int uranusTexture  = loadTexture(FileSystem::getPath("resources/textures/planets/2k_uranus.png").c_str());
    unsigned int neptuneTexture = loadTexture(FileSystem::getPath("resources/textures/planets/2k_neptune.png").c_str());
    unsigned int moonTexture    = loadTexture(FileSystem::getPath("resources/textures/planets/2k_moon.png").c_str());
    unsigned int saturnRingTex  = loadTexture(FileSystem::getPath("resources/textures/planets/2k_saturn_ring_alpha.png").c_str());
    unsigned int specularMap    = loadTexture(FileSystem::getPath("resources/textures/container2_specular.png").c_str());

    // shader configuration
    // --------------------
    lightingShader.use();
    lightingShader.setInt("material.diffuse", 0);
    lightingShader.setInt("material.specular", 1);

    lightCubeShader.use();
    lightCubeShader.setInt("emissiveMap", 0);
    lightCubeShader.setFloat("emissionStrength", 1.0f);

    ringShader.use();
    ringShader.setInt("ringTexture", 0);

    // ------------------------------------------------------------------
    // Solar system scene parameters
    // ------------------------------------------------------------------
    // The sun stays fixed at the origin (and is the only point light source).
    // All eight planets orbit the sun in the XZ-plane. The moon orbits the
    // earth, parented to the earth's transform, so it follows the earth
    // around the sun automatically.
    //
    // Sizes / distances are stylised for visibility (the real ratios would
    // make the gas giants the size of beach balls and Neptune off-screen).
    // The orbital and rotational *periods* however are taken from real
    // astronomical data, so the relative ratios — Mars-year vs Earth-year,
    // tidal locking, retrograde Venus/Uranus rotation, etc. — stay correct.
    //
    // Real periods used (in days):
    //   Body      Sidereal day     Orbital period
    //   Mercury    58.65            87.97
    //   Venus    -243.02           224.70           (retrograde)
    //   Earth       1.00           365.256
    //   Mars        1.026          686.97
    //   Jupiter     0.4135        4332.59
    //   Saturn      0.444        10759.22
    //   Uranus     -0.718        30688.50           (retrograde)
    //   Neptune     0.671        60182.00
    //   Sun        25.05            —
    //   Moon       27.3217          27.3217         (tidally locked to Earth)
    // ------------------------------------------------------------------
    const glm::vec3 sunPos(0.0f, 0.0f, 0.0f);
    const float sunScale = 1.5f;

    const float TWO_PI = 6.28318530718f;
    const float sunSpinPerDay = TWO_PI / 25.05f;

    // The eight planets, ordered outward from the sun.
    // EARTH_INDEX is captured so the moon's frame can be parented to it.
    const std::vector<Planet> planets = {
        // name      texture          orbitR  scale  orbitPerDay              spinPerDay
        { "Mercury", mercuryTexture,   5.0f,  0.25f, TWO_PI / 87.97f,         TWO_PI / 58.65f   },
        { "Venus",   venusTexture,     8.0f,  0.55f, TWO_PI / 224.70f,        TWO_PI / -243.02f },
        { "Earth",   earthTexture,    12.0f,  0.60f, TWO_PI / 365.256f,       TWO_PI / 1.00f    },
        { "Mars",    marsTexture,     16.0f,  0.40f, TWO_PI / 686.97f,        TWO_PI / 1.026f   },
        { "Jupiter", jupiterTexture,  22.0f,  1.40f, TWO_PI / 4332.59f,       TWO_PI / 0.4135f  },
        { "Saturn",  saturnTexture,   28.0f,  1.20f, TWO_PI / 10759.22f,      TWO_PI / 0.444f   },
        { "Uranus",  uranusTexture,   34.0f,  0.85f, TWO_PI / 30688.50f,      TWO_PI / -0.718f  },
        { "Neptune", neptuneTexture,  40.0f,  0.85f, TWO_PI / 60182.00f,      TWO_PI / 0.671f   },
    };
    const size_t EARTH_INDEX  = 2;
    const size_t SATURN_INDEX = 5;

    // Earth's moon (still parented to the earth, so it rides along).
    const float moonScale        = 0.18f;
    const float moonOrbitRadius  = 1.1f;
    const float moonOrbitPerDay  = TWO_PI / 27.3217f;
    const float moonSpinPerDay   = moonOrbitPerDay;   // tidally locked

    // Time-scale presets (real seconds per simulated day):
    //   Normal: 1 day every 2 sec   (one Earth year takes ~12 min,
    //                                full Neptune year ~33 hours -- you won't see it!)
    //   Fast  : 1 day every 30/365.256 sec ≈ 0.0821 s  (one Earth year per 30 real seconds)
    const float SECONDS_PER_SIM_DAY_NORMAL = 2.0f;
    const float SECONDS_PER_SIM_DAY_FAST   = 30.0f / 365.256f;

    // Accumulated simulated time (in days). Driven by deltaTime each frame.
    float simDays = 0.0f;

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // deep-space background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // be sure to activate shader when setting uniforms/drawing objects
        lightingShader.use();
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setFloat("material.shininess", 32.0f);

        /*
           Lights:
           - dirLight: a soft ambient/star-field fill so the dark side of the earth
             isn't pitch black.
           - pointLights[0]: the SUN. Bright, warm, attenuates with distance.
           - pointLights[1..3]: unused for now; zeroed out so they don't contribute.
           - spotLight: disabled (zeroed) for now.
        */
        // directional light (faint star-field fill)
        lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        lightingShader.setVec3("dirLight.ambient", 0.02f, 0.02f, 0.03f);
        lightingShader.setVec3("dirLight.diffuse", 0.05f, 0.05f, 0.07f);
        lightingShader.setVec3("dirLight.specular", 0.1f, 0.1f, 0.1f);

        // point light 0 = the SUN.
        // Attenuation softened so the outer planets (Uranus at 17, Neptune at 20)
        // still receive a visible amount of light.
        lightingShader.setVec3("pointLights[0].position", sunPos);
        lightingShader.setVec3("pointLights[0].ambient",  0.10f, 0.10f, 0.08f);
        lightingShader.setVec3("pointLights[0].diffuse",  1.0f,  0.95f, 0.85f);
        lightingShader.setVec3("pointLights[0].specular", 1.0f,  1.0f,  0.9f);
        lightingShader.setFloat("pointLights[0].constant",  1.0f);
        lightingShader.setFloat("pointLights[0].linear",    0.014f);
        lightingShader.setFloat("pointLights[0].quadratic", 0.0007f);

        // point lights 1..3: disabled for now
        for (int i = 1; i < 4; ++i)
        {
            std::string p = "pointLights[" + std::to_string(i) + "].";
            lightingShader.setVec3(p + "position", glm::vec3(0.0f));
            lightingShader.setVec3(p + "ambient",  glm::vec3(0.0f));
            lightingShader.setVec3(p + "diffuse",  glm::vec3(0.0f));
            lightingShader.setVec3(p + "specular", glm::vec3(0.0f));
            lightingShader.setFloat(p + "constant",  1.0f);
            lightingShader.setFloat(p + "linear",    0.09f);
            lightingShader.setFloat(p + "quadratic", 0.032f);
        }

        // spotLight: disabled (zero contribution) for this scene
        lightingShader.setVec3("spotLight.position", camera.Position);
        lightingShader.setVec3("spotLight.direction", camera.Front);
        lightingShader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
        lightingShader.setVec3("spotLight.diffuse",  0.0f, 0.0f, 0.0f);
        lightingShader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
        lightingShader.setFloat("spotLight.constant",  1.0f);
        lightingShader.setFloat("spotLight.linear",    0.09f);
        lightingShader.setFloat("spotLight.quadratic", 0.032f);
        lightingShader.setFloat("spotLight.cutOff",      glm::cos(glm::radians(12.5f)));
        lightingShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                                0.1f, 300.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        // shared specular map for earth/moon
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        glBindVertexArray(sphereVAO);

        // ------------------------------------------------------------------
        // Build per-body model matrices using a transformation hierarchy.
        //
        //   sunFrame        = T(sunPos)
        //   planetFrame[i]  = sunFrame    * Ry(orbitAngle_i) * T(orbitRadius_i, 0, 0)
        //   planetModel[i]  = planetFrame * Ry(spinAngle_i)  * S(scale_i)
        //   moonFrame       = earthFrame  * Ry(moonOrbit)    * T(moonRadius, 0, 0)
        //   moonModel       = moonFrame   * Ry(moonSpin)     * S(moonScale)
        //   sunModel        = sunFrame    * Ry(sunSpin)      * S(sunScale)
        //
        // Because the moon's frame multiplies onto the earth's frame, the moon
        // automatically rides along with the earth as it orbits the sun.
        // ------------------------------------------------------------------

        // Choose the current time-scale: hold SPACE → fast-forward (1 year per 30 real seconds),
        // otherwise normal (2 real seconds per simulated day).
        const bool fastForward = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        const float secondsPerSimDay = fastForward
            ? SECONDS_PER_SIM_DAY_FAST
            : SECONDS_PER_SIM_DAY_NORMAL;

        // Advance the simulation clock. All angles are derived from this single
        // accumulator, so switching speed mid-flight is seamless.
        simDays += deltaTime / secondsPerSimDay;

        const glm::vec3 yAxis(0.0f, 1.0f, 0.0f);
        const glm::mat4 sunFrame = glm::translate(glm::mat4(1.0f), sunPos);

        // Helper lambda: build the orbital frame for a planet (sun → orbit → outward).
        auto makePlanetFrame = [&](const Planet& p) {
            glm::mat4 frame = sunFrame;
            frame = glm::rotate(frame, simDays * p.orbitPerDay, yAxis);
            frame = glm::translate(frame, glm::vec3(p.orbitRadius, 0.0f, 0.0f));
            return frame;
        };

        // --- draw all PLANETS (lit by the sun) ---
        for (size_t i = 0; i < planets.size(); ++i)
        {
            const Planet& p = planets[i];
            glm::mat4 frame = makePlanetFrame(p);

            // model = frame * spin * scale
            glm::mat4 model = frame;
            model = glm::rotate(model, simDays * p.spinPerDay, yAxis);
            model = glm::scale(model, glm::vec3(p.scale));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.texture);
            lightingShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }

        // Compute earth and Saturn frames explicitly for the moon and the rings.
        // Doing this outside the planet loop removes any ambiguity about whether
        // the right frame got captured.
        const glm::mat4 earthFrame  = makePlanetFrame(planets[EARTH_INDEX]);
        const glm::mat4 saturnFrame = makePlanetFrame(planets[SATURN_INDEX]);

        // --- draw MOON (parented to earthFrame) ---
        {
            float moonOrbitAngle = simDays * moonOrbitPerDay;
            float moonSpinAngle  = simDays * moonSpinPerDay;

            glm::mat4 moonFrame = earthFrame;
            moonFrame = glm::rotate(moonFrame, moonOrbitAngle, yAxis);
            moonFrame = glm::translate(moonFrame, glm::vec3(moonOrbitRadius, 0.0f, 0.0f));

            glm::mat4 model = moonFrame;
            model = glm::rotate(model, moonSpinAngle, yAxis);
            model = glm::scale(model, glm::vec3(moonScale));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, moonTexture);
            lightingShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }

        // --- draw SUN as a self-illuminated body using the simple light shader ---
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
        {
            float sunSpinAngle = simDays * sunSpinPerDay;

            glm::mat4 model = sunFrame;
            model = glm::rotate(model, sunSpinAngle, yAxis);
            model = glm::scale(model, glm::vec3(sunScale));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sunTexture);
            lightCubeShader.setMat4("model", model);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }

        // --- draw SATURN'S RINGS (alpha-blended, drawn last so blending sorts) ---
        // The ring lies in Saturn's local XZ plane. We tilt it ~26.7 deg around
        // the local Z axis so it doesn't appear edge-on. The ring slowly rotates
        // around Saturn (radians/day, much slower than Saturn's spin).
        {
            ringShader.use();
            ringShader.setMat4("projection", projection);
            ringShader.setMat4("view", view);
            ringShader.setVec3("lightPos",     sunPos);
            ringShader.setVec3("lightColor",   1.0f, 0.95f, 0.85f);
            ringShader.setVec3("ambientColor", 0.10f, 0.10f, 0.10f);

            // Tilt around Z so the ring tips out of the orbital plane.
            const float saturnAxialTilt = glm::radians(26.73f);
            // A gentle ring rotation just for visual life (not physical).
            const float ringSpinAngle   = simDays * (TWO_PI / 0.6f) * 0.05f;

            glm::mat4 model = saturnFrame;
            model = glm::rotate(model, saturnAxialTilt, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, ringSpinAngle,   yAxis);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, saturnRingTex);
            ringShader.setMat4("model", model);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);          // don't write depth so subsequent
                                            // transparent layers (none for now)
                                            // would still sort correctly

            glBindVertexArray(ringVAO);
            glDrawElements(GL_TRIANGLES, ringIndexCount, GL_UNSIGNED_INT, 0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &ringVAO);
    glDeleteBuffers(1, &ringVBO);
    glDeleteBuffers(1, &ringEBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// generate a UV-sphere mesh into vertices (8 floats / vertex: pos, normal, uv) and indices.
// ------------------------------------------------------------------------------------------
void generateSphere(std::vector<float>& vertices,
                    std::vector<unsigned int>& indices,
                    float radius,
                    unsigned int sectorCount,
                    unsigned int stackCount)
{
    vertices.clear();
    indices.clear();

    const float PI = 3.14159265358979323846f;
    float sectorStep = 2.0f * PI / static_cast<float>(sectorCount);
    float stackStep  = PI / static_cast<float>(stackCount);

    // vertices
    //
    // Convention: Y is the pole axis (north pole at +Y, south pole at -Y).
    // The equator lies in the XZ-plane.  This matches how equirectangular
    // textures are authored: the top row of the image is the north pole,
    // the bottom row is the south pole, and U sweeps eastward around the equator.
    for (unsigned int i = 0; i <= stackCount; ++i)
    {
        float stackAngle = PI / 2.0f - static_cast<float>(i) * stackStep; // +pi/2 (north) .. -pi/2 (south)
        float xz = radius * cosf(stackAngle);                              // ring radius in xz-plane
        float y  = radius * sinf(stackAngle);                              // height along pole axis

        for (unsigned int j = 0; j <= sectorCount; ++j)
        {
            float sectorAngle = static_cast<float>(j) * sectorStep; // 0 .. 2pi (longitude)

            float x = xz * cosf(sectorAngle);
            float z = xz * sinf(sectorAngle);

            // position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            // normal (unit sphere => normalized position)
            vertices.push_back(x / radius);
            vertices.push_back(y / radius);
            vertices.push_back(z / radius);
            // texture coords:  u = longitude (0..1 west-to-east),
            //                  v = 0 at top of texture (north pole), 1 at bottom (south pole)
            vertices.push_back(static_cast<float>(j) / static_cast<float>(sectorCount));
            vertices.push_back(static_cast<float>(i) / static_cast<float>(stackCount));
        }
    }

    // indices (two triangles per quad, skipping degenerate triangles at the poles)
    for (unsigned int i = 0; i < stackCount; ++i)
    {
        unsigned int k1 = i * (sectorCount + 1);
        unsigned int k2 = k1 + sectorCount + 1;

        for (unsigned int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

// generate a flat annulus (ring) mesh in the local XZ plane.
// Vertices use the same 8-float layout as the sphere (pos, normal, uv).
// u sweeps radially from 0 (innerRadius) to 1 (outerRadius); v is fixed at 0.5
// so a 1-D radial ring texture is sampled correctly.
// ----------------------------------------------------------------------------
void generateRing(std::vector<float>& vertices,
                  std::vector<unsigned int>& indices,
                  float innerRadius,
                  float outerRadius,
                  unsigned int sectorCount)
{
    vertices.clear();
    indices.clear();

    const float PI = 3.14159265358979323846f;
    float sectorStep = 2.0f * PI / static_cast<float>(sectorCount);

    for (unsigned int j = 0; j <= sectorCount; ++j)
    {
        float angle = static_cast<float>(j) * sectorStep;
        float c = cosf(angle);
        float s = sinf(angle);

        // inner ring vertex
        vertices.push_back(innerRadius * c);
        vertices.push_back(0.0f);
        vertices.push_back(innerRadius * s);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f); // normal +Y
        vertices.push_back(0.0f); vertices.push_back(0.5f);                            // u=0 (inner)

        // outer ring vertex
        vertices.push_back(outerRadius * c);
        vertices.push_back(0.0f);
        vertices.push_back(outerRadius * s);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f); // normal +Y
        vertices.push_back(1.0f); vertices.push_back(0.5f);                            // u=1 (outer)
    }

    // each segment forms a quad (inner_j, outer_j, inner_j+1, outer_j+1) → 2 triangles
    for (unsigned int j = 0; j < sectorCount; ++j)
    {
        unsigned int i0 = j * 2;       // inner_j
        unsigned int i1 = j * 2 + 1;   // outer_j
        unsigned int i2 = j * 2 + 2;   // inner_j+1
        unsigned int i3 = j * 2 + 3;   // outer_j+1

        indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
        indices.push_back(i2); indices.push_back(i1); indices.push_back(i3);
    }
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
