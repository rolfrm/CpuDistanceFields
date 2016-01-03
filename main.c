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
  logd("\n");
  raise(SIGKILL);
  exit(255);
}
inline float sphere_distance(vec3 p, vec3 sp, float r);
float sphere_distance(vec3 p, vec3 sp, float r){
  vec3 d = vec3_sub(p, sp);
  return vec3_len(d) - r;
}

float img[512][512] = {0};
vec3 sphere_center, sphere_center2, sphere_center3;
float sphere_radius = 0.5;
vec3 camera_center = {0};

void blit_img(float _x, float _y, float v){
  int px = (int) (_x / M_PI * 2 * 512 + 256);
  int py = (int) (_y / M_PI * 2 *512 + 256);

  if(px >= 0 && px < 256 && py >= 0 && py < 256)
    img[px][py] = v;
}


float distance_function(vec3 p){
  
  p.z = p.z / 18.0;
  p.z = p.z - floor(p.z);
  p.z = p.z * 18;

  p.y = (p.y + 5) / 20.0;
  p.y = p.y - floor(p.y);
  p.y = p.y * 20 - 5;
  
  vec3 d = vec3_sub(p, sphere_center);
  float v = vec3_len(d) - sphere_radius * 1.3;//r2;
  

  //return v;
  vec3 d2 = vec3_sub(p, sphere_center2);
  float v2 = vec3_len(d2) - sphere_radius * 1.4;

  return MIN(v2, v);
  
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
  float r_dist = sinf(aov * 0.5) * cam_dist;
  //vec3_print(new_point);logd("sd: %f aov:%f cam_dist:%f  ", sd, aov, cam_dist); logd("Rdist: %f\n", r_dist);
  if(r_dist < sd || aov <= 1.0 / 256){
    dist += sd;
    goto start;
    //render_img(x, y, dist + sd, aov);
  }else{
    float aov_half = aov * 0.5;
    //logd("1 ");
    dist = dist + sd  - r_dist;
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
#include <iron/mem.h>
typedef struct{
  float * distance;
  bool * is_ended;
  float * last_distance;
  int * active_children;
  vec3 * dirvec;
  int size;
  int total_size;
  float cell_fov;
}img_map;

int get_lods(int size){
  return log2(size) + 1;
}

img_map * create_maps(int lods, float f){
  img_map lod_maps[lods];
  vec3 horz2 = vec3_normalize(vec3mk(1,0,f));
  vec3 horz1 = vec3_normalize(vec3mk(-1,0,f));
  float fov = vec3_len(vec3_sub(horz2, horz1));
  for(int i = 0; i < lods; i++){
    lod_maps[i].size = 1 << i;
    lod_maps[i].total_size = lod_maps[i].size * lod_maps[i].size;
    lod_maps[i].is_ended = alloc0( lod_maps[i].total_size * sizeof(bool));
    lod_maps[i].active_children = alloc0( lod_maps[i].total_size * sizeof(int));
    lod_maps[i].last_distance = alloc0( lod_maps[i].total_size * sizeof(float));
    lod_maps[i].distance = alloc0( lod_maps[i].total_size * sizeof(float));
    lod_maps[i].dirvec = alloc0( lod_maps[i].total_size * sizeof(vec3));
    lod_maps[i].cell_fov = fov / lod_maps[i].size;
    float s1 = lod_maps[i].size + 1;
    for(int y = 0; y < lod_maps[i].size; y++){
      for(int x = 0; x < lod_maps[i].size; x++){
	float _y = 2.0 * ((y + 1) / s1 - 0.5);
	float _x = 2.0 * ((x + 1) / s1 - 0.5);

	lod_maps[i].dirvec[y * lod_maps[i].size + x] = vec3_normalize(vec3mk(_x, _y, f));
      }
    }    
  }
  return iron_clone(lod_maps, sizeof(lod_maps));
}

void render_img2(img_map * lod_maps, int lods, float * img){
  float sqrt2 = sqrtf(2.0);
  UNUSED(img);
  for(int i = 0; i < lods; i++){
    memset(lod_maps[i].distance, 0, lod_maps[i].total_size * sizeof(float));
    memset(lod_maps[i].is_ended, 0, lod_maps[i].total_size * sizeof(bool));
  }
  void handle_node(int lod, int x, int y){
    img_map * map = lod_maps + lod;
    img_map * submap = lod_maps + lod + 1;
    int offset = x + y * map->size;
    vec3 d = map->dirvec[offset];
    //logd("Handle node: %i %i %i: %i\n ", lod, x, y, offset);

    while(true){
      //iron_usleep(10000);      
      float dd = map->distance[offset];
      vec3 p = vec3_scale(d, dd);
      float d2 = distance_function(p);
      //logd("iterate: %i %i %i: %f %f\n ", lod, x, y, dd,  d2);
      float newdist = dd + d2;
      if(lod < lods - 1){
	float r_dist = map->cell_fov * map->distance[offset] * sqrt2 * 0.25;
	if(d2 < r_dist){
	  //logd("going down: %f\n", r_dist);
	  int s2 = submap->size;
	  int _x = x * 2;
	  int _y = y * 2;
	  int c1 = _x + _y * s2;
	  int c2 = c1 + 1;
	  int c3 = _x + (_y + 1) * s2;
	  int c4 = c3 + 1;
	  //logd(".. %f %f %f\n", d2, r_dist, newdist);
	  if(!submap->is_ended[c1] && submap->distance[c1] <= (newdist - r_dist)){
	    submap->distance[c1] = newdist - r_dist;
	    handle_node(lod + 1,_x,_y);
	  }
	  if(!submap->is_ended[c2] && submap->distance[c2] <= (newdist - r_dist)){
	    submap->distance[c2] = newdist - r_dist;
	    handle_node(lod + 1,_x + 1, _y);
	  }
	  if(!submap->is_ended[c3] && submap->distance[c3] <= (newdist - r_dist)){
	    submap->distance[c3] = newdist - r_dist;
	    handle_node(lod + 1, _x, _y + 1);
	  }
	  if(!submap->is_ended[c4] && submap->distance[c4] <= (newdist - r_dist)){
	    submap->distance[c4] = newdist - r_dist;
	    handle_node(lod + 1, _x + 1, _y + 1);
	  }
	  //logd(" -- %i %i %i %i\n", submap->is_ended[c1] , submap->is_ended[c2] ,
	  // submap->is_ended[c3] , submap->is_ended[c4]);
	  if(submap->is_ended[c1] && submap->is_ended[c2] &&
	     submap->is_ended[c3] && submap->is_ended[c4]){
	    map->is_ended[offset] = true;
	    map->distance[offset] = submap->distance[c1];
	    
	    return;
	  }else{

	    int offsets[] = {c1, c2, c3, c4};
	    float mind = 0;
	    for(int i = 0; i < 4; i++){
	      //logd("Is ended: %i %f\n", submap->is_ended[offsets[i]], submap->distance[offsets[i]]); 
	      if(submap->is_ended[offsets[i]] == false){
		mind = MAX(submap->distance[offsets[i]], mind);
	      }
	    }
	    if(mind <= map->distance[offset])
	    map->distance[offset] = mind;
	    //ASSERT(false);
	    continue;
	  }
	}
      }else{
	if(d2 < 0.0001){
	  map->is_ended[offset] = true;
	  img[offset] = 1;
	  //logd("Hit object\n");
	  return;
	}
	/*float r_dist = submap->cell_fov * newdist * sqrt2;
	//logd("??? ! %f %f %f\n", r_dist, d2, newdist);
	if(r_dist * 2 < d2){ 
	  map->distance[offset] = newdist;
	  //logd("This happens! %f %f %f\n", r_dist, d2, newdist);
	  return;
	  }*/
      }
      map->distance[offset] = newdist;
      if( map->distance[offset] > 400.0){
	map->is_ended[offset] = true;
	//logd("Hit end\n");
	return;
      }
      
      //vec3_print(p); vec3_print(d);logd(" : %f \n", d2); 
      //vec3_print(lod_maps[1].dirvec[0]); vec3_print(lod_maps[1].dirvec[1]);
    }
  }
  handle_node(0, 0, 0);
}

#include <signal.h>

void handle_sigint(int signum){
  ERROR("Caught sigint %i\n", signum);
  //signal(SIGKILL, NULL); // next time just quit.
}

int main(){
  sphere_center = vec3mk(0.0,0.2,5.0);
  sphere_center2 = vec3mk(0.0,-0.2,5.0);
  sphere_center3 = vec3mk(0.2,0.0,0.9);
  light = vec3mk(0.0,0,0);
  int lods = get_lods(512);
  img_map * maps = create_maps(lods, 1.0);
  SDL_Window * window;
  SDL_Renderer * renderer;
  //vec3 p = vec3mk(0.0, 0.0, 0.0);
  //vec3 d = vec3_normalize(vec3mk(1.0, 1.0, 1.0));
  //render_img2((float *)img, 256,256, 1);
  //return 0;
  
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
  window = SDL_CreateWindow("--", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512,512 ,SDL_WINDOW_SHOWN  /*| SDL_WINDOW_OPENGL*/);
  renderer = SDL_CreateRenderer(window, -1, 0);

  /* Creating the surface. */
  SDL_Surface *s = SDL_CreateRGBSurface(0, 512, 512, 32, 0, 0, 0, 0);
  
  SDL_Texture *bitmapTex = NULL;
  float t = 0.0;
  signal(SIGINT, NULL);
  signal(SIGINT, handle_sigint);
  for(int i = 0; i < 10000; i++){
    
    memset(img,0, sizeof(img));
    u64 ts = timestamp();
    render_img2(maps, lods, (float *) img);
    logd("time: %f ms\n", ((float)(timestamp()- ts)) * 0.001 );
    t += 0.01;
    //light.y = sin(t);
    //sphere_center.y = cos(t) * 0.5;
    sphere_center.x = cos(t * 1.3) * 4.5;
    sphere_center2.x = cos(t * 1.1) * 4.5;
  //return 0;
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 0, 0, 0));
    SDL_SetRenderDrawColor( renderer, 128, 100, 64, 255 );
    for(int i = 0; i < 512; i++){
      for(int j = 0; j < 512; j++){
	SDL_Rect r = {i, j, 1, 1};
	if(img[i][j] != 0){
	  int v = 255 * img[i][j];
	  //
	  SDL_FillRect(s, &r, SDL_MapRGB(s->format, v/2, v/6 * 2, v / 5));
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
