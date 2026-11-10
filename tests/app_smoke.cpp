// Linux/GCC integration probe: real application sources, synthetic keys, read-only frame observation.
// Linker wrappers are tied to the pinned engine ABI; no test switches exist in production code.
#ifdef main
#undef main
#endif
#include <SDL3/SDL.h>
#include <engine/engine.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <thread>
using namespace std::chrono_literals;
namespace {
std::array<std::atomic<bool>, SDL_SCANCODE_COUNT> keyboard{};
std::atomic<bool> finished{false}, failed{false};
struct View { bool ready=false, visible=false, replay=false, droneHit=false; float x=0,y=0,vy=0; } view;
std::mutex viewMutex;
View current() { std::lock_guard lock(viewMutex); return view; }
void keys(std::initializer_list<SDL_Scancode> codes) {
  for (auto& key : keyboard) key=false;
  for (auto code : codes) keyboard[code]=true;
}
template<class Predicate> void waitFor(Predicate predicate, std::chrono::milliseconds timeout,
                                      const char* reason) {
  const auto end=std::chrono::steady_clock::now()+timeout;
  while (!finished && std::chrono::steady_clock::now()<end) {
    if (predicate()) return;
    std::this_thread::sleep_for(5ms);
  }
  throw std::runtime_error(reason);
}
void tap(SDL_Scancode key) { keys({key}); std::this_thread::sleep_for(160ms); keys({}); std::this_thread::sleep_for(160ms); }
void quit() { keys({}); SDL_Event event{}; event.type=SDL_EVENT_QUIT; SDL_PushEvent(&event); }
void script() {
  try {
    waitFor([] { return current().ready; },30s,"application did not publish initial frame");
    const std::string role=std::getenv("SMOKE_ROLE")?std::getenv("SMOKE_ROLE"):"drone";
    if (role=="observer") {
      waitFor([] { pollfd input{0,POLLIN,0}; return poll(&input,1,0)>0; },60s,"observer shutdown signal");
      quit(); return;
    }
    std::this_thread::sleep_for(1300ms);
    if (role=="drone") {
      // Measured in four real-app runs. State feedback handles render/input latency and orbit phase.
      keys({SDL_SCANCODE_D});
      waitFor([] { return current().x>=180; },2500ms,"first jump approach");
      keys({SDL_SCANCODE_D,SDL_SCANCODE_SPACE}); std::this_thread::sleep_for(160ms);
      keys({SDL_SCANCODE_D}); std::this_thread::sleep_for(500ms);
      waitFor([] { return current().y<=623; },1500ms,"moving platform height");
      waitFor([] { return current().vy==0; },1500ms,"moving platform landing");
      waitFor([] { return current().x>=435; },2000ms,"second jump approach");
      keys({}); std::this_thread::sleep_for(160ms);
      keys({SDL_SCANCODE_D,SDL_SCANCODE_SPACE}); std::this_thread::sleep_for(160ms);
      keys({SDL_SCANCODE_D}); std::this_thread::sleep_for(500ms);
      waitFor([] { return current().y<=433; },1500ms,"high platform height");
      waitFor([] { return current().vy==0; },1500ms,"high platform landing");
      waitFor([] { return current().x>=900; },2500ms,"drone approach"); keys({});
      waitFor([] { return !current().visible; },8s,"drone did not kill robot");
      if (!current().droneHit) throw std::runtime_error("disappearance was not a drone collision");
      waitFor([] { return current().visible; },4s,"timed respawn not displayed");
      std::puts("SMOKE_DRONE hit hidden respawn");
    } else {
      tap(SDL_SCANCODE_F5);
      keys({SDL_SCANCODE_A});
      waitFor([] { return !current().visible; },15s,"death-zone route did not kill robot"); keys({});
      waitFor([] { return current().visible; },4s,"actor did not respawn");
      tap(SDL_SCANCODE_F5); tap(SDL_SCANCODE_F6);
      waitFor([] { return current().replay; },2s,"recorded replay not displayed");
      waitFor([] { return !current().replay; },12s,"replay did not finish naturally");
      tap(SDL_SCANCODE_F6);
      waitFor([] { return current().replay; },2s,"second replay not displayed");
      tap(SDL_SCANCODE_ESCAPE);
      waitFor([] { return !current().replay; },2s,"manual replay stop not displayed");
      std::puts("SMOKE_REPLAY natural manual");
    }
    quit();
  } catch (const std::exception& error) {
    std::fprintf(stderr,"SMOKE_FAILURE %s\n",error.what()); failed=true; quit();
  }
}
}
extern "C" const bool* __wrap_SDL_GetKeyboardState(int* count) {
  thread_local std::array<bool,SDL_SCANCODE_COUNT> copy{};
  for (std::size_t i=0;i<copy.size();++i) copy[i]=keyboard[i];
  if (count) *count=static_cast<int>(copy.size());
  return copy.data();
}
// Only two C++ ABI names are intercepted, behind the Linux/GNU CMake gate.
using Messages=std::deque<engine::networking::receivedMessage>;
void realReceive(engine::events::eventManager*,Messages,std::int64_t)
  asm("__real__ZN6engine6events12eventManager7receiveESt5dequeINS_10networking15receivedMessageESaIS4_EEl");
void wrapReceive(engine::events::eventManager*,Messages,std::int64_t)
  asm("__wrap__ZN6engine6events12eventManager7receiveESt5dequeINS_10networking15receivedMessageESaIS4_EEl");
void wrapReceive(engine::events::eventManager* self,Messages messages,std::int64_t tick) {
  if (std::getenv("SMOKE_DROP_EVENTS")) { if (!messages.empty()) std::puts("SMOKE_DROPPED"); messages.clear(); }
  realReceive(self,std::move(messages),tick);
}
bool realFrame(engine::Renderer&,const engine::Scene&,engine::TextureId)
  asm("__real__ZN7factory17renderReplayFrameERN6engine8RendererERKNS0_5SceneEj");
bool wrapFrame(engine::Renderer&,const engine::Scene&,engine::TextureId)
  asm("__wrap__ZN7factory17renderReplayFrameERN6engine8RendererERKNS0_5SceneEj");
bool wrapFrame(engine::Renderer& renderer,const engine::Scene& scene,engine::TextureId texture) {
  const bool replay=realFrame(renderer,scene,texture);
  static engine::EntityId robot=0;
  static int lastRemote=-1;
  static std::string lastNotice;
  View next=current(); next.replay=replay; next.visible=false;
  int remote=0;
  for (const auto& [id,shape] : scene.shapes()) {
    if (shape.color.r==72 && shape.color.g==205 && shape.color.b==230) { robot=id; next.visible=true; }
    if (shape.color.r==255 && shape.color.g==210 && shape.color.b==60) ++remote;
  }
  if (robot && scene.hasEntity(robot)) {
    const auto p=scene.transform(robot).position; next.x=p.x; next.y=p.y;
    if (const auto* body=scene.getRigidBody(robot)) next.vy=body->velocity.y;
  }
  for (const auto& [id,text] : scene.texts()) {
    if (text.val.starts_with("STATUS:")) next.ready=true;
    if (text.val=="STATUS: DRONE HIT - TRY AGAIN") next.droneHit=true;
    if (text.val.starts_with("STATUS: PLAYER ") && text.val!=lastNotice) {
      lastNotice=text.val; std::printf("SMOKE_NOTICE %s\n",text.val.c_str());
    }
  }
  if (!replay && remote!=lastRemote) { lastRemote=remote; std::printf("SMOKE_REMOTE %d\n",remote); }
  { std::lock_guard lock(viewMutex); view=next; }
  std::fflush(stdout);
  return replay;
}
int game_main(int,char**);
int main(int argc,char** argv) {
  std::thread input(script);
  const int result=game_main(argc,argv); finished=true; input.join();
  return result?result:failed?1:0;
}
