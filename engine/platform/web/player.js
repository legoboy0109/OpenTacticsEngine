// Shared loader of the web player (index.html and embed.html).
// Needs ote-player.js (the Emscripten module factory createOtePlayer) loaded first.
//
//   OtePlayer.run({
//     persist: true,                  // mount browser storage at /userdata (options survive reloads)
//     prepare: async (api) => {...},  // fill the virtual file system: api.writeFile(path, bytes),
//                                     //   api.status(text), api.fetchAll(items, concurrency)
//     args: ['--game', 'x'],          // arguments for the player's main()
//     onReady: () => {},              // first frame is up
//     onError: (message) => {},       // anything went wrong (also shown on the page)
//     onLog: (line) => {},            // every line the engine logs (the editor's Play panel reads the test level from it)
//   });
//
// Dependency-free, no build step. Everything the engine logs goes to the
// console; lines marked [ERROR] are also kept and shown in the error panel.
(function () {
  'use strict';

  var errorLines = [];   // engine "[ERROR] ..." lines, newest last
  var failed = false;
  var logHook = null;    // opts.onLog
  var statusEl = null, panelEl = null, canvas = null;

  function el(tag, cls, parent) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (parent) parent.appendChild(e);
    return e;
  }

  function setStatus(text) {
    if (!statusEl) {
      statusEl = el('div', 'ote-status', document.body);
      el('div', 'ote-spinner', statusEl);
      statusEl.appendChild(el('div', 'ote-status-text'));
    }
    statusEl.lastChild.textContent = text;
  }

  function hideStatus() {
    if (statusEl) { statusEl.remove(); statusEl = null; }
  }

  // The readable error panel. `detail` is optional extra text (engine log lines).
  function showError(message, detail) {
    hideStatus();
    if (!panelEl) {
      panelEl = el('div', 'ote-error', document.body);
      el('h1', null, panelEl).textContent = 'The game could not be started';
      el('p', 'ote-error-msg', panelEl);
      el('pre', 'ote-error-detail', panelEl);
      var btn = el('button', null, panelEl);
      btn.textContent = 'Reload';
      btn.onclick = function () { location.reload(); };
    }
    panelEl.querySelector('.ote-error-msg').textContent = message;
    var pre = panelEl.querySelector('.ote-error-detail');
    pre.textContent = detail || '';
    pre.style.display = detail ? '' : 'none';
  }

  // Wraps a failure: panel + console + callback, once.
  function makeFail(opts) {
    return function (message, detail) {
      if (failed) return;
      failed = true;
      console.error('[ote-player] ' + message);
      showError(message, detail === undefined ? errorLines.join('\n') : detail);
      if (opts.onError) opts.onError(message);
    };
  }

  function log(text) {
    var line = String(text);
    if (logHook) { try { logHook(line); } catch (e) { /* a broken hook must not stop the log */ } }
    if (line.indexOf('[ERROR]') === 0) { errorLines.push(line); console.error(line); }
    else if (line.indexOf('[warn]') === 0) console.warn(line);
    else console.log(line);
  }

  // Creates `dir` and its parents in the virtual file system.
  function mkdirs(FS, dir) {
    var parts = dir.split('/').filter(Boolean), cur = '';
    for (var i = 0; i < parts.length; i++) {
      cur += '/' + parts[i];
      if (!FS.analyzePath(cur).exists) FS.mkdir(cur);
    }
  }

  function writeFile(FS, path, bytes) {
    var slash = path.lastIndexOf('/');
    if (slash > 0) mkdirs(FS, path.slice(0, slash));
    FS.writeFile(path, bytes);
  }

  // Runs async job(item) for every item with at most `limit` in flight.
  // Rejects with the first failure (remaining jobs are not started).
  function forEachLimited(items, limit, job) {
    return new Promise(function (resolve, reject) {
      var next = 0, running = 0, done = false;
      function pump() {
        if (done) return;
        if (next >= items.length && running === 0) { done = true; resolve(); return; }
        while (running < limit && next < items.length) {
          running++;
          job(items[next++]).then(function () { running--; pump(); },
                                  function (e) { if (!done) { done = true; reject(e); } });
        }
      }
      pump();
    });
  }

  function fetchBytes(url) {
    return fetch(url).then(function (r) {
      if (!r.ok) throw new Error('HTTP ' + r.status + ' for ' + url);
      return r.arrayBuffer();
    }).then(function (b) { return new Uint8Array(b); });
  }

  // Page set-up shared by both pages: no zoom/scroll gestures, keys that the
  // game uses must not move the page, keyboard focus on the canvas.
  function setupCanvas() {
    canvas = document.getElementById('canvas');
    if (!canvas) { canvas = el('canvas', null, document.body); canvas.id = 'canvas'; }
    canvas.tabIndex = 0;
    canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    function focus() { try { window.focus(); canvas.focus({ preventScroll: true }); } catch (e) {} }
    canvas.addEventListener('pointerdown', focus);
    window.addEventListener('pointerdown', focus);
    document.addEventListener('touchmove', function (e) { e.preventDefault(); }, { passive: false });
    window.addEventListener('keydown', function (e) {
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      if (/^(Arrow(Up|Down|Left|Right)| |Tab|Backspace)$/.test(e.key)) e.preventDefault();
    }, true);
    window.addEventListener('wheel', function (e) { e.preventDefault(); }, { passive: false });
    window.focusCanvas = focus;
    return canvas;
  }

  function run(opts) {
    var fail = makeFail(opts);
    window.addEventListener('error', function (e) { fail('Unexpected error: ' + (e.message || e.error)); });
    window.addEventListener('unhandledrejection', function (e) {
      fail('Unexpected error: ' + (e.reason && e.reason.message ? e.reason.message : e.reason));
    });
    if (typeof createOtePlayer !== 'function') {
      fail('ote-player.js did not load (is the web build next to this page?)');
      return;
    }
    setStatus('Starting...');
    setupCanvas();
    var Module;
    var args = opts.args || [];
    logHook = opts.onLog || null;

    createOtePlayer({
      canvas: canvas,
      print: log,
      printErr: log,
      onAbort: function (what) { fail('The player stopped: ' + what); },
    }).then(function (m) {
      Module = m;
      var FS = m.FS;
      if (opts.persist) {
        // Browser storage for settings.json: load it now, the engine saves with FS.syncfs.
        try {
          FS.mkdir('/userdata');
          FS.mount(m.IDBFS, {}, '/userdata');
        } catch (e) { log('[warn] browser storage unavailable: ' + e); return null; }
        return new Promise(function (resolve) {
          FS.syncfs(true, function (err) {
            if (err) log('[warn] cannot read saved options: ' + err);
            resolve();
          });
        });
      }
    }).then(function () {
      var FS = Module.FS;
      return opts.prepare({
        writeFile: function (path, bytes) { writeFile(FS, path, bytes); },
        status: setStatus,
        fetchBytes: fetchBytes,
    
      });
    }).then(function () {
      setStatus('Starting game...');
      var errorsBefore = errorLines.length;
      Module.callMain(args);  // returns once the main loop is installed
      // Two animation frames: the engine has drawn at least once.
      return new Promise(function (resolve) {
        requestAnimationFrame(function () { requestAnimationFrame(resolve); });
      }).then(function () { return errorsBefore; });
    }).then(function (errorsBefore) {
      if (failed) return;
      hideStatus();
      window.focusCanvas();
      if (errorLines.length > errorsBefore) {
        // The engine started but reported errors (it shows its own error screen).
        // Keep that screen visible; tell the host.
        failed = true;
        var msg = errorLines.slice(errorsBefore).join('\n');
        if (opts.onError) opts.onError(msg);
        return;
      }
      document.documentElement.setAttribute('data-ote', 'ready');
      window.__oteReadyMs = performance.now();
      if (opts.onReady) opts.onReady();
    }).catch(function (e) {
      fail(e && e.message ? e.message : String(e));
    });
  }

  // Removes // and /* */ comments (the engine's JSON files allow them) so that
  // JSON.parse works. Strings are respected.
  function stripJsonComments(text) {
    var out = '', i = 0, n = text.length;
    while (i < n) {
      var c = text[i], d = text[i + 1];
      if (c === '"') {
        var j = i + 1;
        while (j < n && text[j] !== '"') j += text[j] === '\\' ? 2 : 1;
        out += text.slice(i, j + 1);
        i = j + 1;
      } else if (c === '/' && d === '/') {
        while (i < n && text[i] !== '\n') i++;
      } else if (c === '/' && d === '*') {
        var k = text.indexOf('*/', i + 2);
        i = k < 0 ? n : k + 2;
      } else { out += c; i++; }
    }
    return out;
  }

  window.OtePlayer = {
    run: run,
    stripJsonComments: stripJsonComments,
    fetchBytes: fetchBytes,
    forEachLimited: forEachLimited,
  };
})();
