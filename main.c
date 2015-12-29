#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h> //chdir
#include <time.h> //difftime
#include <math.h>

#include <stdint.h>
#include <iron/types.h>
#include <iron/log.h>
#include <iron/utils.h>
#include <iron/time.h>
#include <iron/linmath.h>

void _error(const char * file, int line, const char * msg, ...){
  char buffer[1000];  
  va_list arglist;
  va_start (arglist, msg);
  vsprintf(buffer,msg,arglist);
  va_end(arglist);
  loge("%s\n", buffer);
  loge("Got error at %s line %i\n", file,line);
  iron_log_stacktrace();
  raise(SIGSTOP);
  exit(255);
}
inline float sphere_distance(vec3 p, vec3 sp, float r);
float sphere_distance(vec3 p, vec3 sp, float r){
  vec3 d = vec3_sub(p, sp);
  return vec3_len(d) - r;
}

float img[256][256] = {0};
vec3 sphere_center, sphere_center2, sphere_center3;
float sphere_radius = 0.5;
vec3 camera_center = {0};

void blit_img(float _x, float _y, float v){
  int px = (int) (_x / M_PI * 2 * 256 + 128);
  int py = (int) (_y / M_PI * 2 *256 + 128);

  if(px >= 0 && px < 256 && py >= 0 && py < 256)
    img[px][py] = v;
}

  
float distance_function(vec3 p){

  vec3 d = vec3_sub(p, sphere_center);
  float v = vec3_len(d) - sphere_radius;//r2;
  
  vec3 d2 = vec3_sub(p, sphere_center2);
  float v2 = vec3_len(d2) - sphere_radius;
  return MAX(v, v2);
  
  UNUSED(v2);
  /*
  float d1 = sphere_distance(p, sphere_center, sphere_radius);
  float d2 = sphere_distance(p, sphere_center2, sphere_radius);
  float d3 = sphere_distance(p, sphere_center3, sphere_radius);*/

  return sqrtf(v);
  
 
  //return d2;
  //return MIN(d1, d2);
  //return MAX(d3, MAX(d1,d2));
}
vec3 light;
int calculations = 0;
void render_img(float x, float y, float dist, float aov){
 start:
  calculations++;
  vec3 d = vec3_normalize(vec3mk(x , y , 1));
  vec3 p = vec3_scale(d, dist);
  float sd = distance_function(p);

  if(sd < 0.1 && aov < 1.0 / 256.0){
    vec3 nx = p, ny = p, nz = p;
    nx.x += 0.01;
    ny.y += 0.01;
    nz.z += 0.01;
    float sdx = distance_function(nx) - sd;
    float sdy = distance_function(ny) - sd;
    float sdz = distance_function(nz) - sd;
    vec3 n = vec3mk(-sdx,-sdy,-sdz);
    //logd("Normal: %f", sdz)vec3_print(n);logd("\n");
    n = vec3_normalize(n);

    vec3 vl = vec3_normalize(vec3_sub(p, light));
    UNUSED(vl);
    //vl = vec3_normalize(vec3mk(0,0,1));
    float vldot = MIN(1, MIN(1, MAX(0, 0.5 * vec3_mul_inner(n, vl))) + 0.25);
    //if(vldot < 0) return;
    UNUSED(sdz);UNUSED(sdy);
    blit_img(x, y, vldot );
    return;
  }
  vec3 dp = vec3_scale(d, sd);
  vec3 new_point = vec3_add(p, dp);  
  float cam_dist = vec3_len(vec3_sub(new_point, camera_center));

  if(cam_dist > 4.0)
    return;
  float r_dist = sinf(aov * 0.5) * cam_dist * 3;
  //vec3_print(new_point);logd("sd: %f aov:%f cam_dist:%f  ", sd, aov, cam_dist); logd("Rdist: %f\n", r_dist);
  if(r_dist < sd || aov <= 1.0 / 256){
    dist += sd;
    goto start;
    //render_img(x, y, dist + sd, aov);
  }else{
    float aov_half = aov * 0.5;
    //logd("1 ");
    dist += sd * 0;
    render_img(x - aov_half, y - aov_half, dist, aov_half);
    //logd("2 ");
    render_img(x + aov_half, y - aov_half, dist, aov_half);
    //logd("3 ");
    render_img(x - aov_half, y + aov_half, dist, aov_half);
    //logd("4 ");
    x += aov_half;
    y += aov_half;
    aov = aov_half;
    goto start;
    //render_img(x + aov_half, y + aov_half, dist, aov_half);
  }
  
}
#include <SDL2/SDL.h>

int main(){
  sphere_center = vec3mk(0.0,0.0,1.0);
  sphere_center2 = vec3mk(0.0,-1,1.0);
  sphere_center3 = vec3mk(0.2,0.0,0.9);
  light = vec3mk(0.0,0,0);
  SDL_Window * window;
  SDL_Renderer * renderer;
  //vec3 p = vec3mk(0.0, 0.0, 0.0);
  //vec3 d = vec3_normalize(vec3mk(1.0, 1.0, 1.0));

  SDL_Init(SDL_INIT_EVERYTHING);
  window = SDL_CreateWindow("--", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256,256 ,SDL_WINDOW_SHOWN /*| SDL_WINDOW_OPENGL*/);
  renderer = SDL_CreateRenderer(window, -1, 0);

  /* Creating the surface. */
  SDL_Surface *s = SDL_CreateRGBSurface(0, 256, 256, 32, 0, 0, 0, 0);
  
  SDL_Texture *bitmapTex = NULL;
  float t = 0.0;
  for(int i = 0; i < 1000; i++){
    memset(img,0, sizeof(img));
    u64 ts = timestamp();
    render_img(0, 0, 0, M_PI * 0.25);
    logd("time: %f ms\n", ((float)(timestamp()- ts)) * 0.001 );
    t += 0.005;
    //light.y = sin(t);
    sphere_center.y = cos(t) * 0.5;
    sphere_center.x = sin(t) * 0.5;
    sphere_center2.y = cos(t * 1.1 + 1.2) * 0.5;
  //return 0;
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 0, 0, 0));
    for(int i = 0; i < 256; i++){
      for(int j = 0; j < 256; j++){
	SDL_Rect r = {i, j, 1, 1};
	if(img[i][j] != 0){
	  int v = 255 * img[i][j];
	  //SDL_SetRenderDrawColor( renderer, v, v, v, 255 );
	  SDL_FillRect(s, &r, SDL_MapRGB(s->format, v, v, v));
	  //SDL_RenderFillRect( renderer, &r );
	  //SDL_RenderDrawPoint(renderer, i,j);
	  
	}
	//logd("?? %i %i\n", i, j);
	//logd("%c",'0' + img[i][j]);
      }
      //logd("\n");
    }
    bitmapTex = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
	break;
      }	    
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, bitmapTex, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(bitmapTex);
    //iron_usleep(10000);
    }

  
  logd("Calculations: %i\n", calculations);
  return 0;
}
