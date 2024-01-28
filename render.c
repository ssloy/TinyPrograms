// render.c
// a port of Dmitry Sokolov's 70 lines raytracer

#define GL_width  80
#define GL_height 50
#include "GL_tty.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

static inline void vcopy(float* to, const float* from) { memcpy(to, from, sizeof(float)*3); }
static inline float dot(const float* u, const float* v) { return u[0]*v[0]+u[1]*v[1]+u[2]*v[2]; }
static inline float distance2(const float* p1, const float* p2) {
    return (p1[0]-p2[0])*(p1[0]-p2[0]) + (p1[1]-p2[1])*(p1[1]-p2[1]) + (p1[2]-p2[2])*(p1[2]-p2[2]);
}
#define VEC(V) V[c]
#define VECOP(E) for(int c=0; c<3; ++c) E

bool box_intersect(
    const float* bmin, const float* bmax, const float* ray_origin, const float* ray_direction,
    float* normal, float* point
) {
    for(int i=0; i<3; ++i) { // for each coordinate axis
        if(fabs(ray_direction[i]) < 1e-3) continue; // avoid divide by 0
        normal[0] = normal[1] = normal[2] = 0.0;  // here we test against 3 planes (instead of 6), i.e.
        normal[i] = ray_direction[i] > 0 ? -1 : 1;  // no rendering from the inside ofa box
        float d  = ((ray_direction[i] > 0 ? bmin[i] : bmax[i]) - ray_origin[i]) / ray_direction[i];
        VECOP(VEC(point) = VEC(ray_origin) + VEC(ray_direction)*d);
        if( d>0 && point[(i+1)%3] > bmin[(i+1)%3] && point[(i+1)%3] < bmax[(i+1)%3] &&
                   point[(i+2)%3] > bmin[(i+2)%3] && point[(i+2)%3] < bmax[(i+2)%3] ) {
            return true;
        }
    }
    return false;
}

bool sphere_intersect(
    const float* center, float radius, const float* ray_origin, const float* ray_direction,
    float* normal, float* point
) {
    float V[3] = { center[0]-ray_origin[0], center[1]-ray_origin[1], center[2]-ray_origin[2] };
    float proj = dot(ray_direction, V);
    float delta = radius*radius + proj*proj - dot(V,V);
    if(delta > 0){
        float d = proj - sqrt(delta);
        if(d > 0) {
            VECOP(VEC(point) = VEC(ray_origin) + VEC(ray_direction)*d);
            VECOP(VEC(normal) = (VEC(point) - VEC(center))/radius);
            return true;
        }
    }
    return false;
}

bool scene_intersect(
    const float* ray_origin, const float* ray_direction,
    float* point, float* normal, float* color
) {
    #define NOBJ 5
    static struct { float color[3]; float p1[3]; float p2[3];  float radius; } O[NOBJ] = {
        { {1.,.4,.6}, {6,0,7     }, {0,0,0    }, 2   },
        { {1.,1.,.3}, {2.8, 1.1,7}, {0,0,0    }, 0.9 },
        { {2.,2.,2.}, {5,-10,-7  }, {0,0,0    }, 8   }, // color > 1 -> lamp
        { {.4,.7,1.}, {3,-4,11   }, {7,2,13   }, 0   }, // radius 0  -> box
        { {.6,.7,.6}, {0,2,6     }, {11,2.2,16}, 0   }  // radius 0  -> box
    };
    float nearest = 1e30;
    float p[3];
    float N[3];
    bool scene_has_isect = false;
    for(int o=0; o<NOBJ; ++o) {
        bool has_isect = false;
        if(O[o].radius == 0.0) {
            has_isect = box_intersect(O[o].p1, O[o].p2, ray_origin, ray_direction, p, N);
        } else {
            has_isect = sphere_intersect(O[o].p1, O[o].radius, ray_origin, ray_direction, p, N);
        }
        if(has_isect) {
            scene_has_isect = true;
            float d = distance2(ray_origin, p);
            if(d < nearest) { nearest = d; vcopy(point, p); vcopy(normal,N), vcopy(color,O[o].color); }
        }
    }
    return scene_has_isect;
}

float urand() { return 2.0*((float)(random() & 65535)/65535.0) - 1.0; }

void reflect(const float* I, const float* N, float* R) {
    float w = 2*dot(I,N);
    VECOP(VEC(R) = VEC(I) - w*VEC(N)); //  + urand() / 6.0);
    float l = sqrt(dot(R,R));
    VECOP(VEC(R) *= l);
}

float ambient_color[3] = { .5, .5, .5 };
float light_color[3] = {1.0, 1.0, 1.0};
float focal = 60; float azimuth = 30.*M_PI/180.;
int nrays = 1; /* 10 */ int maxdepth = 3;

void trace(const float* eye, const float* ray, int depth, int maxdepth, float* rgb) {
    if(depth > maxdepth) { vcopy(rgb, ambient_color); return; }
    float point[3]; float normal[3]; float color[3];
    if(!scene_intersect(eye, ray, point, normal, color)) { vcopy(rgb, ambient_color); return; }
    if(color[0] > 1.0) { vcopy(rgb, light_color); return; } // if we hit a lamp -> white
    float color2[3], ray2[3];
    reflect(ray, normal, ray2);
    trace(point, ray2, depth+1, maxdepth, color2);
    VECOP(VEC(rgb) = VEC(color) * VEC(color2));
}

void render(int X, int Y, float* r, float* g, float *b) {
    static float zero[3] = {0,0,0};
    float rgb[3] = { 0.0, 0.0, 0.0 };
    float ray[3] = { (float)X-(float)GL_width/2., (float)Y-(float)GL_height/2., focal};
    float s = 1.0/sqrt(dot(ray,ray));
    VECOP(VEC(ray) = s * VEC(ray));
    float x =  cos(azimuth)*ray[0] + sin(azimuth)*ray[2]; // rotate the ray 30 degrees around Y-axis
    float z = -sin(azimuth)*ray[0] + cos(azimuth)*ray[2]; 
    ray[0] = x; ray[2] = z;
    for(int r=0; r<nrays; ++r) {
        float rgb_ray[3];
        trace(zero, ray, 0, maxdepth, rgb_ray);
        VECOP(VEC(rgb) += VEC(rgb_ray));
    }
    *r = rgb[0] / (float)nrays;
    *g = rgb[1] / (float)nrays;
    *b = rgb[2] / (float)nrays;
}

int main() {
    GL_init();
    GL_scan_RGBf(GL_width, GL_height, render);
    GL_terminate();
    return 0;
}
