#include "core/utils.h"
#include "core/dck.h"

#include <raylib.h>
#include <raymath.h>

#include <math.h>
#include <string.h>

#define TEX_RES 32

#define UNIT_SCALE  ((Vector3) { 1.0f, 1.0f, 1.0f })

Matrix
matrix_from(Vector3 position, Vector3 rotation_axis, float rotation_angle, Vector3 scale)
{
    Matrix mat_scale       = MatrixScale(scale.x, scale.y, scale.z);
    Matrix mat_rotation    = MatrixRotate(rotation_axis, rotation_angle * DEG2RAD);
    Matrix mat_translation = MatrixTranslate(position.x, position.y, position.z);

    return MatrixMultiply(MatrixMultiply(mat_scale, mat_rotation), mat_translation);
}

static Material render_mesh_material;
static i32 render_mesh_normal_matrix_loc;

void
render_mesh(Mesh mesh, Shader shader, Texture2D texture, Matrix matrix)
{
    render_mesh_material.shader = shader;
    SetMaterialTexture(&render_mesh_material, MATERIAL_MAP_ALBEDO, texture);

    Matrix normal_matrix = MatrixTranspose(MatrixInvert(matrix));
    SetShaderValueMatrix(shader, render_mesh_normal_matrix_loc, normal_matrix);

    DrawMesh(mesh, render_mesh_material, matrix);
}

Shader
load_shader(const char *vertex_path, const char *fragment_path)
{
    Shader shader = LoadShader(vertex_path, fragment_path); 
    if (!IsShaderReady(shader)) {
        fprintf(stderr, "Failed to loaded shader: (%s, %s)\n", vertex_path, fragment_path);
        exit(1);
    }
    printf("Loaded shader: (%s, %s)\n", vertex_path, fragment_path);

    return shader;
}

typedef struct
{
    dck_stretchy_t (Vector3, u32) positions;
    dck_stretchy_t (Vector3, u32) normals;
    dck_stretchy_t (Vector2, u32) texcoords;
} mb_t;

void
mb_clear(mb_t *mb)
{
    mb->positions.count = 0;
    mb->normals.count   = 0;
    mb->texcoords.count = 0;
}

Mesh
mb_to_mesh(mb_t *mb)
{
    Mesh mesh = {
        .vertexCount   = mb->positions.count,
        .triangleCount = mb->positions.count / 3,

        .vertices  = (f32 *)(mb->positions.data),
        .texcoords = (f32 *)(mb->texcoords.data),
        .normals   = (f32 *)(mb->normals.data),
    };

    UploadMesh(&mesh, false);

    return mesh;
}

void
mb_vertex(mb_t *mb, Vector3 position, Vector3 normal, Vector2 texcoord)
{
    dck_stretchy_push(mb->positions, position);
    dck_stretchy_push(mb->normals,   normal);
    dck_stretchy_push(mb->texcoords, texcoord);
}

typedef struct
{
    mb_t *mb;

    Vector3 positions[4];
    Vector3 normals  [4];
    Vector2 texcoords[4];

    u32 count;

    u32 vertex_start;
} mb_strip_t;

mb_strip_t
mb_strip_create(mb_t *mb)
{
    return (mb_strip_t) {
        .mb           = mb,
        .vertex_start = mb->positions.count,
    };
}

void
mb_strip_push(mb_strip_t *strip, Vector3 position, Vector3 normal, Vector2 texcoord)
{
    strip->positions[strip->count] = position;
    strip->normals  [strip->count] = normal;
    strip->texcoords[strip->count] = texcoord;

    strip->count++;

    if (strip->count == 4) {
        strip->count = 2;

        mb_vertex(strip->mb, strip->positions[0],
                             strip->normals  [0],
                             strip->texcoords[0]);

        mb_vertex(strip->mb, strip->positions[1],
                             strip->normals  [1],
                             strip->texcoords[1]);

        mb_vertex(strip->mb, strip->positions[2],
                             strip->normals  [2],
                             strip->texcoords[2]);

        mb_vertex(strip->mb, strip->positions[3],
                             strip->normals  [3],
                             strip->texcoords[3]);

        mb_vertex(strip->mb, strip->positions[2],
                             strip->normals  [2],
                             strip->texcoords[2]);

        mb_vertex(strip->mb, strip->positions[1],
                             strip->normals  [1],
                             strip->texcoords[1]);

        strip->positions[0] = strip->positions[2];
        strip->normals  [0] = strip->normals  [2];
        strip->texcoords[0] = strip->texcoords[2];

        strip->positions[1] = strip->positions[3];
        strip->normals  [1] = strip->normals  [3];
        strip->texcoords[1] = strip->texcoords[3];
    }
}

void
mb_strip_reset(mb_strip_t *strip)
{
    strip->count = 0;
}

typedef struct
{
    u32 vertex_start;
    u32 vertex_count;
} mb_view_t;

mb_view_t
mb_strip_get_view(mb_strip_t *strip)
{
    return (mb_view_t) {
        .vertex_start = strip->vertex_start,
        .vertex_count = strip->mb->positions.count - strip->vertex_start,
    };
}

mb_view_t
mb_view_begin(mb_t *mb)
{
    return (mb_view_t) {
        .vertex_start = mb->positions.count,
    };
}

mb_view_t
mb_view_end(mb_t *mb, mb_view_t view)
{
    return (mb_view_t) {
        .vertex_start = view.vertex_start,
        .vertex_count = mb->positions.count - view.vertex_start,
    };
}

mb_view_t
mb_view_copy(mb_t *mb_dst, mb_t *mb_src, mb_view_t view_src)
{
    u32 vertex_start = mb_dst->positions.count;

    dck_stretchy_reserve(mb_dst->positions, view_src.vertex_count);
    dck_stretchy_reserve(mb_dst->normals,   view_src.vertex_count);
    dck_stretchy_reserve(mb_dst->texcoords, view_src.vertex_count);

    memcpy(mb_dst->positions.data + mb_dst->positions.count,
           mb_src->positions.data + view_src.vertex_start,
           view_src.vertex_count * sizeof(Vector3));

    memcpy(mb_dst->normals.data + mb_dst->positions.count,
           mb_src->normals.data + view_src.vertex_start,
           view_src.vertex_count * sizeof(Vector3));

    memcpy(mb_dst->texcoords.data + mb_dst->positions.count,
           mb_src->texcoords.data + view_src.vertex_start,
           view_src.vertex_count * sizeof(Vector2));

    mb_dst->positions.count += view_src.vertex_count;
    mb_dst->normals.count   += view_src.vertex_count;
    mb_dst->texcoords.count += view_src.vertex_count;

    return (mb_view_t) {
        .vertex_start = vertex_start,
        .vertex_count = view_src.vertex_count,
    };
}

void
mb_view_transform(mb_t *mb, mb_view_t view, Matrix matrix)
{
    Matrix normal_matrix = MatrixTranspose(MatrixInvert(matrix));

    for (u32 i = view.vertex_start; i < view.vertex_start + view.vertex_count; ++i) {
        mb->positions.data[i] = Vector3Transform(mb->positions.data[i], matrix);
        mb->normals.data[i]   = Vector3Transform(mb->normals.data[i], normal_matrix);
    }
}

mb_view_t
mb_view_dupe(mb_t *mb, mb_view_t view, Matrix matrix)
{
    mb_view_t new  = mb_view_copy(mb, mb, view);
    mb_view_transform(mb, new, matrix);
    return new;
}

Texture2D
load_texture(const char *path)
{
    Texture2D texture = LoadTexture(path);
    if (!IsTextureReady(texture)) {
        fprintf(stderr, "Failed to load texture: %s!\n", path);
        exit(1);
    }
    printf("Loaded texture: %s\n", path);

    return texture;
}

mb_view_t
create_face(mb_t *mb, f32 xs, f32 ys)
{
    mb_strip_t strip = mb_strip_create(mb);

    f32 y_left = ys;

    for (i32 yi = 0; y_left > 0.0f; ++yi, y_left -= 1.0f) {
        f32 height = y_left;
        if (height > 1.0f) {
            height = 1.0f;
        }

        f32 x_left = xs;

        for (i32 xi = 0; x_left > 0.0f; ++xi, x_left -= 1.0f) {
            f32 width = x_left;
            if (width > 1.0f) {
                width = 1.0f;
            }

            mb_strip_reset(&strip);

            f32 x = (f32)xi;
            f32 y = (f32)yi;

            mb_strip_push(&strip,
                (Vector3) { x,    y,    0.0f },
                (Vector3) { 0.0f, 0.0f, 1.0f },
                (Vector2) { 0.0f, 0.0f }
            );

            mb_strip_push(&strip,
                (Vector3) { x + width, y,    0.0f },
                (Vector3) { 0.0f,      0.0f, 1.0f },
                (Vector2) { width,     0.0f }
            );

            mb_strip_push(&strip,
                (Vector3) { x,    y + height, 0.0f },
                (Vector3) { 0.0f, 0.0f,       1.0f },
                (Vector2) { 0.0f, height }
            );

            mb_strip_push(&strip,
                (Vector3) { x + width, y + height, 0.0f },
                (Vector3) { 0.0f,      0.0f,       1.0f },
                (Vector2) { width,     height }
            );
        }
    }

    return mb_strip_get_view(&strip);
}

mb_view_t
create_plank(mb_t *mb, f32 xs, f32 ys, f32 zs)
{
    mb_view_t view = mb_view_begin(mb);

    mb_view_t front = create_face(mb, xs, ys);
    mb_view_dupe(mb, front, matrix_from(
        (Vector3) { 0.0f, ys,   -zs },
        (Vector3) { 1.0f, 0.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t bottom = create_face(mb, xs, zs);
    mb_view_transform(mb, bottom, matrix_from(
        (Vector3) { 0.0f, 0.0f, -zs },
        (Vector3) { 1.0f, 0.0f, 0.0f }, 90.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));
    mb_view_dupe(mb, bottom, matrix_from(
        (Vector3) { 0.0f, ys,   -zs },
        (Vector3) { 1.0f, 0.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t end = create_face(mb, zs, ys);
    mb_view_transform(mb, end, matrix_from(
        (Vector3) { 0.0f, 0.0f, -zs },
        (Vector3) { 0.0f, 1.0f, 0.0f },-90.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));
    mb_view_dupe(mb, end, matrix_from(
        (Vector3) { xs,   0.0f, -zs },
        (Vector3) { 0.0f, 1.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    return mb_view_end(mb, view);
}

mb_view_t
create_plank_angled(mb_t *mb, f32 xs, f32 ys, f32 zs, f32 a1, f32 a2)
{
    mb_view_t view = mb_view_begin(mb);

    mb_view_t bottom = create_face(mb, xs, zs);
    mb_view_transform(mb, bottom, matrix_from(
        (Vector3) { 0.0f, 0.0f, -zs },
        (Vector3) { 1.0f, 0.0f, 0.0f }, 90.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    f32 wd1 = (ys / sinf(a1 * DEG2RAD)) * sinf((90.0f - a1) * DEG2RAD);
    f32 wd2 = (ys / sinf(a2 * DEG2RAD)) * sinf((90.0f - a2) * DEG2RAD);

    // TODO: Implement a proper triangle routine and use it instead.

    mb_vertex(mb, (Vector3) { 0.0f, 0.0f, 0.0f },
                  (Vector3) { 0.0f, 0.0f, 1.0f },
                  (Vector2) { 0.0f, 0.0f });

    mb_vertex(mb, (Vector3) { wd1,  0.0f, 0.0f },
                  (Vector3) { 0.0f, 0.0f, 1.0f },
                  (Vector2) { wd1,  0.0f });

    mb_vertex(mb, (Vector3) { wd1,  ys,   0.0f },
                  (Vector3) { 0.0f, 0.0f, 1.0f },
                  (Vector2) { wd1,  ys });


    mb_vertex(mb, (Vector3) { 0.0f, 0.0f, -zs },
                  (Vector3) { 0.0f, 0.0f, -1.0f },
                  (Vector2) { 0.0f, 0.0f });

    mb_vertex(mb, (Vector3) { wd1,  ys,   -zs },
                  (Vector3) { 0.0f, 0.0f, -1.0f },
                  (Vector2) { wd1,  ys });

    mb_vertex(mb, (Vector3) { wd1,  0.0f, -zs },
                  (Vector3) { 0.0f, 0.0f, -1.0f },
                  (Vector2) { wd1,  0.0f });


    mb_vertex(mb, (Vector3) { xs - wd2, 0.0f, 0.0f },
                  (Vector3) { 0.0f,     0.0f, 1.0f },
                  (Vector2) { 0.0f,     0.0f });

    mb_vertex(mb, (Vector3) { xs,   0.0f, 0.0f },
                  (Vector3) { 0.0f, 0.0f, 1.0f },
                  (Vector2) { wd2, 0.0f });

    mb_vertex(mb, (Vector3) { xs - wd2, ys,   0.0f },
                  (Vector3) { 0.0f,     0.0f, 1.0f },
                  (Vector2) { 0.0f,     ys });


    mb_vertex(mb, (Vector3) { xs - wd2, 0.0f, -zs },
                  (Vector3) { 0.0f,     0.0f, -1.0f },
                  (Vector2) { 0.0f,     0.0f });

    mb_vertex(mb, (Vector3) { xs - wd2, ys,   -zs },
                  (Vector3) { 0.0f,     0.0f, -1.0f },
                  (Vector2) { 0.0f,     ys });

    mb_vertex(mb, (Vector3) { xs,   0.0f, -zs },
                  (Vector3) { 0.0f, 0.0f, -1.0f },
                  (Vector2) { wd2, 0.0f });

    f32 rat1 = sqrtf(wd1 * wd1 + ys * ys) / ys;
    f32 rat2 = sqrtf(wd2 * wd2 + ys * ys) / ys;

    mb_view_t end1 = create_face(mb, zs, ys);
    mb_view_transform(mb, end1, matrix_from(
        (Vector3) { 0.0f, 0.0f, -zs },
        (Vector3) { 0.0f, 1.0f, 0.0f },-90.0f,
        (Vector3) { 1.0f, rat1, 1.0f }
    ));
    mb_view_transform(mb, end1, matrix_from(
        (Vector3) { 0.0f, 0.0f, 0.0f },
        (Vector3) { 0.0f, 0.0f, 1.0f }, -a1,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t end2 = create_face(mb, zs, ys);
    mb_view_transform(mb, end2, matrix_from(
        (Vector3) { 0.0f, 0.0f, 0.0f },
        (Vector3) { 0.0f, 1.0f, 0.0f }, 90.0f,
        (Vector3) { 1.0f, rat2, 1.0f }
    ));
    mb_view_transform(mb, end2, matrix_from(
        (Vector3) { xs,   0.0f, 0.0f },
        (Vector3) { 0.0f, 0.0f, 1.0f }, a2,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t side = create_face(mb, xs - wd1 - wd2, ys);
    mb_view_transform(mb, side, matrix_from(
        (Vector3) { wd1,  0.0f, 0.0f },
        (Vector3) { 0.0f, 0.0f, 1.0f }, 0.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));
    mb_view_dupe(mb, side, matrix_from(
        (Vector3) { 0.0f, ys, -zs },
        (Vector3) { 1.0f, 0.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t top = create_face(mb, xs - wd1 - wd2, zs);
    mb_view_transform(mb, top, matrix_from(
        (Vector3) { wd1,  ys, 0.0f },
        (Vector3) { 1.0f, 0.0f, 0.0f }, -90.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    return mb_view_end(mb, view);
}

mb_view_t
create_wall(mb_t *mb)
{
    f32 lw = 0.12f;
    f32 sw = 0.06f;

    f32 height = 2.0f;
    f32 width  = 1.5f - lw * 2.0f;

    mb_view_t full = mb_view_begin(mb);

    mb_view_t side = mb_view_begin(mb);
    {
        mb_view_t plank = create_plank(mb, height, lw, sw);
        mb_view_transform(mb, plank, matrix_from(
            (Vector3) { lw,   0.0f, 0.0f },
            (Vector3) { 0.0f, 0.0f, 1.0f }, 90.0f,
            (Vector3) { 1.0f, 1.0f, 1.0f }
        ));

        mb_view_dupe(mb, plank, matrix_from(
            (Vector3) { 0.0f, 0.0f, -(lw + sw) },
            (Vector3) { 0.0f, 1.0f,  0.0f }, 0.0f,
            (Vector3) { 1.0f, 1.0f,  1.0f }
        ));

        mb_view_t plank_middle = create_plank(mb, height - 2.0f * (lw - sw), sw, lw);
        mb_view_transform(mb, plank_middle, matrix_from(
            (Vector3) { lw, (lw - sw), -sw },
            (Vector3) { 0.0f,  0.0f, 1.0f }, 90.0f,
            (Vector3) { 1.0f,  1.0f, 1.0f }
        ));
    }
    side = mb_view_end(mb, side);

    mb_view_dupe(mb, side, matrix_from(
        (Vector3) { width + (lw * 2.0f), 0.0f, -(lw + 2.0f * sw) },
        (Vector3) { 0.0f, 1.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    mb_view_t bottom = mb_view_begin(mb);
    {
        mb_view_t plank = create_plank(mb, width, lw, sw);
        mb_view_transform(mb, plank, matrix_from(
            (Vector3) { lw,   0.0f, 0.0f },
            (Vector3) { 0.0f, 1.0f, 0.0f }, 0.0f,
            (Vector3) { 1.0f, 1.0f, 1.0f }
        ));

        mb_view_dupe(mb, plank, matrix_from(
            (Vector3) { 0.0f, lw - sw, -(sw + lw) },
            (Vector3) { 1.0f, 0.0f, 0.0f }, 90.0f,
            (Vector3) { 1.0f, 1.0f, 1.0f }
        ));

        mb_view_dupe(mb, plank, matrix_from(
            (Vector3) { 0.0f, 0.0f, -(sw + lw) },
            (Vector3) { 0.0f, 1.0f, 0.0f }, 0.0f,
            (Vector3) { 1.0f, 1.0f, 1.0f }
        ));
    }
    bottom = mb_view_end(mb, bottom);

    mb_view_dupe(mb, bottom, matrix_from(
        (Vector3) { 0.0f, height, -(lw + 2.0f * sw) },
        (Vector3) { 1.0f, 0.0f,   0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f,   1.0f }
    ));

    f32 angled_width  = width / 4.0f;
    f32 angled_length = sqrtf(angled_width * angled_width * 2.0f);

    mb_view_t angleds = mb_view_begin(mb);
    {
        f32 inset = -(sw + (lw - sw) * 0.5f);

        mb_view_t angled = create_plank_angled(mb, angled_length, lw, sw, 45.0f, 45.0f);
        mb_view_transform(mb, angled, matrix_from(
            (Vector3) { lw + angled_width, lw, inset },
            (Vector3) { 0.0f,              0.0f, 1.0f }, 90.0f + 45.0f,
            (Vector3) { 1.0f,              1.0f, 1.0f }
        ));

        angled = create_plank_angled(mb, angled_length, lw, sw, 45.0f, 45.0f);
        mb_view_transform(mb, angled, matrix_from(
            (Vector3) { lw,   height - lw - angled_width, inset },
            (Vector3) { 0.0f, 0.0f, 1.0f }, 45.0f,
            (Vector3) { 1.0f, 1.0f, 1.0f }
        ));
    }
    angleds = mb_view_end(mb, angleds);
    mb_view_dupe(mb, angleds, matrix_from(
        (Vector3) { width + lw * 2.0f, 0.0f, -(lw + sw * 2.0f) },
        (Vector3) { 0.0f, 1.0f, 0.0f }, 180.0f,
        (Vector3) { 1.0f, 1.0f, 1.0f }
    ));

    return mb_view_end(mb, full);
}

i32
main(void)
{
    SetTraceLogLevel(LOG_WARNING);

    InitWindow(1920, 1080, "CAD");
    SetTargetFPS(60);

    render_mesh_material = LoadMaterialDefault();

    Texture2D texture = load_texture("res/wood_100.png");

    Shader based_shader = load_shader("res/shaders/based.vert.glsl",
                                      "res/shaders/based.frag.glsl");
    render_mesh_normal_matrix_loc = GetShaderLocation(based_shader, "normalMatrix");

    f32 angle = 0.0f;

    mb_t mb = {0};

    create_wall(&mb);

    Mesh mesh = mb_to_mesh(&mb);

    mb_clear(&mb);

    Camera3D camera = {
        .position   = { 0.0f, 0.0f, 0.0f },
        .target     = { 0.0f, 0.0f,-1.0f },
        .up         = { 0.0f, 1.0f, 0.0f },
        .fovy       = 50.f,
        .projection = CAMERA_PERSPECTIVE,
    };

    f32 dt;

    Vector2 orientation = {0};

    b32 floating     = false;
    u32 move_timeout = 0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_Q))
            break;

        dt = GetFrameTime();

        angle += dt * 60.0f;

        f32 speed = 6.0f;

        if (IsKeyPressed(KEY_H)) {
            floating = !floating;

            if (floating) {
                DisableCursor();
            }
            else {
                EnableCursor();
            }
        }

        if (IsKeyPressed(KEY_F)) {
            ToggleBorderlessWindowed();
            move_timeout = 3;
        }
        else {
            if (move_timeout) {
                --move_timeout;
            }
        }

        if (floating) {
            int screen_width = GetScreenWidth();

            if (move_timeout == 0) {
                orientation = Vector2Add(orientation, Vector2Scale(GetMouseDelta(), 3.0f / screen_width));
            }

            f32 cos_a = cosf(orientation.x);
            f32 sin_a = sinf(orientation.x);

            if (IsKeyDown(KEY_W)) {
                camera.position.z -= cos_a * dt * speed;
                camera.position.x += sin_a * dt * speed;
            }
            if (IsKeyDown(KEY_S)) {
                camera.position.z += cos_a * dt * speed;
                camera.position.x -= sin_a * dt * speed;
            }
            if (IsKeyDown(KEY_A)) {
                camera.position.x -= cos_a * dt * speed;
                camera.position.z -= sin_a * dt * speed;
            }
            if (IsKeyDown(KEY_D)) {
                camera.position.x += cos_a * dt * speed;
                camera.position.z += sin_a * dt * speed;
            }
            if (IsKeyDown(KEY_SPACE)) {
                camera.position.y += dt * speed;
            }
            if (IsKeyDown(KEY_LEFT_SHIFT)) {
                camera.position.y -= dt * speed;
            }

            Vector3 view_dir = Vector3Transform(
                (Vector3) { 0.0f, 0.0,-1.0f },
                MatrixMultiply(MatrixRotateX(-orientation.y), MatrixRotateY(-orientation.x))
            );

            camera.target = Vector3Add(camera.position, view_dir);
        }

        BeginDrawing();
            ClearBackground(GRAY);

            BeginMode3D(camera);
                Vector3 position      = { 0.0f, 0.0f,-5.0f };
                Vector3 rotation_axis = { 0.0f, 1.0f, 0.0f };
                Vector3 scale         = { 1.0f, 1.0f, 1.0f };

                Matrix matrix = matrix_from(position, rotation_axis, 0.0f, scale);
                render_mesh(mesh, based_shader, texture, matrix);
            EndMode3D();

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
