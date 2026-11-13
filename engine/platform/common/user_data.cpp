// commitUserData(): see platform.h. Shared by every backend.
#include "platform/platform.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace ote {

#ifdef __EMSCRIPTEN__
// /userdata is an IDBFS mount (player.js): files written there live in memory
// until FS.syncfs(false) copies them to IndexedDB. Only one sync may run at a
// time, so a request that arrives meanwhile is remembered and run afterwards.
void commitUserData() {
  EM_ASM({
    if (typeof FS === 'undefined' || !FS.analyzePath('/userdata').exists) return;
    var st = Module.__oteSync || (Module.__oteSync = {busy: false, again: false});
    if (st.busy) { st.again = true; return; }
    var run = function() {
      st.busy = true;
      st.again = false;
      FS.syncfs(false, function(err) {
        st.busy = false;
        if (err) console.error('[ERROR] cannot save options to browser storage: ' + err);
        if (st.again) run();
      });
    };
    run();
  });
}
#else
void commitUserData() {}
#endif

}  // namespace ote
