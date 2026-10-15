// Imports of the web build (platform/web.h), linked with --js-library.
addToLibrary({
    // Resolves at the browser's next animation frame, when the canvas is
    // composited, with the frame's time in ms; or with the time after `ms`
    // if no frame comes: browsers pause animation frames for hidden or
    // covered windows, and the game (and the music its cut-scenes follow)
    // must keep time there too.
    web_wait_frame__async: 'auto',
    web_wait_frame: (ms) => new Promise(resolve => {
        let raf;
        const done = t => {
            clearTimeout(timeout);
            cancelAnimationFrame(raf);
            resolve(t);
        };
        const timeout = setTimeout(() => done(performance.now()), ms);
        raf = requestAnimationFrame(done);
    }),

    // Shows the click-to-play screen (#play, styled by the shell) over the
    // canvas until it is clicked or Enter or Space is pressed. The click is
    // the user gesture browsers want before playing sound.
    web_click_to_play__async: 'auto',
    web_click_to_play: () => new Promise(resolve => {
        const el = document.createElement('div');
        el.id = 'play';
        el.innerHTML = '<div class="title">Reunion</div><div class="go">Click to play</div>';
        // Appended now: SDL's fill-document mode hides the page's own elements.
        document.body.appendChild(el);
        const key = e => {
            if (e.code === 'Enter' || e.code === 'Space' || e.code === 'NumpadEnter')
                done(e);
        };
        const done = e => {
            // Not for the game: it starts with no key or click pending.
            e.preventDefault();
            e.stopImmediatePropagation();
            removeEventListener('keydown', key, true);
            el.remove();
            resolve();
        };
        el.addEventListener('click', done);
        addEventListener('keydown', key, true);
    }),

    // Mounts the player's folder (SDL_GetPrefPath's /libsdl/...) from the
    // browser's IndexedDB, and resolves once its files are read. Every change
    // to it is written back on its own (autoPersist), soon after the file is
    // closed.
    web_open_storage__async: 'auto',
    web_open_storage__deps: ['$IDBFS'],
    web_open_storage: () => new Promise(resolve => {
        FS.mkdir('/libsdl');
        FS.mount(IDBFS, { autoPersist: true }, '/libsdl');
        FS.syncfs(true, err => {
            if (err)
                console.error('The saved files could not be read:', err);
            resolve();
        });
    }),

    // The browser's file picker for a ZIP or ISO, or a folder: the files
    // chosen are copied to /tmp/import, replacing the last ones. Resolves
    // with the path of the file or folder there (malloc'd), or 0 if the
    // player cancels.
    web_choose_import__async: 'auto',
    web_choose_import__deps: ['$stringToNewUTF8'],
    web_choose_import: (folder) => new Promise(resolve => {
        const input = document.createElement('input');
        input.type = 'file';
        if (folder)
            input.webkitdirectory = true;
        else
            input.accept = '.zip,.iso';
        input.addEventListener('cancel', () => resolve(0));
        input.addEventListener('change', async () => {
            const remove = path => {
                if (FS.isDir(FS.stat(path).mode)) {
                    for (const name of FS.readdir(path))
                        if (name !== '.' && name !== '..')
                            remove(`${path}/${name}`);
                    FS.rmdir(path);
                } else {
                    FS.unlink(path);
                }
            };
            if (FS.analyzePath('/tmp/import').exists)
                remove('/tmp/import');
            let chosen = 0;
            for (const file of input.files) {
                // A folder's files are named from the folder ("Reunion/GRAFIKA/MAIN.PIC").
                const path = `/tmp/import/${file.webkitRelativePath || file.name}`;
                FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
                FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
                chosen ||= folder ? path.split('/').slice(0, 4).join('/') : path;
            }
            resolve(chosen ? stringToNewUTF8(chosen) : 0);
        });
        input.click();
    }),
});
