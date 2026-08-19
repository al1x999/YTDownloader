document.addEventListener('DOMContentLoaded', () => {
    const API_BASE = 'http://127.0.0.1:8080/api';

    // State
    let currentParsedData = null;
    let selectedType = 'video';
    let selectedFormat = '1080p';
    let selectedAudioFormat = 'mp3';
    let selectedFps = '60';
    let isSmartMode = false;
    let selectedItemIndices = new Set();
    let isSubfolderEnabled = true;

    let historyData = [];
    let activeHistoryFilter = 'all';
    let historySearchQuery = '';

    // Elements
    const btnPasteLink = document.getElementById('btnPasteLink');
    const smartModeSwitch = document.getElementById('smartModeSwitch');
    const speedLimitSelect = document.getElementById('speedLimitSelect');
    const btnOpenFolder = document.getElementById('btnOpenFolder');
    const btnClearCompleted = document.getElementById('btnClearCompleted');
    const downloadList = document.getElementById('downloadList');
    const emptyState = document.getElementById('emptyState');
    const activeCountBadge = document.getElementById('activeCountBadge');
    const queueTotalText = document.getElementById('queueTotalText');
    const totalSpeedText = document.getElementById('totalSpeedText');

    const btnPauseAll = document.getElementById('btnPauseAll');
    const btnResumeAll = document.getElementById('btnResumeAll');
    const btnToolbarLocation = document.getElementById('btnToolbarLocation');
    const toolbarLocationText = document.getElementById('toolbarLocationText');

    // Navigation
    const navDownloads = document.getElementById('navDownloads');
    const navHistory = document.getElementById('navHistory');
    const navSettings = document.getElementById('navSettings');
    const viewDownloads = document.getElementById('viewDownloads');
    const viewHistory = document.getElementById('viewHistory');
    const viewSettings = document.getElementById('viewSettings');
    const historyCountBadge = document.getElementById('historyCountBadge');

    // History View Elements
    const btnClearHistory = document.getElementById('btnClearHistory');
    const statCompletedCount = document.getElementById('statCompletedCount');
    const statTotalSize = document.getElementById('statTotalSize');
    const statHistoryLocation = document.getElementById('statHistoryLocation');
    const historySearchInput = document.getElementById('historySearchInput');
    const historyFilterPills = document.querySelectorAll('.history-filter-pill');
    const historyEmptyState = document.getElementById('historyEmptyState');
    const historyList = document.getElementById('historyList');

    // Settings Elements
    const settingsOutputDir = document.getElementById('settingsOutputDir');
    const btnSaveOutputDir = document.getElementById('btnSaveOutputDir');
    const settingsSubfolderCheck = document.getElementById('settingsSubfolderCheck');
    const settingsFpsSelect = document.getElementById('settingsFpsSelect');
    const settingsFormatSelect = document.getElementById('settingsFormatSelect');
    const settingsMaxConcurrent = document.getElementById('settingsMaxConcurrent');

    // Modals
    const parseModal = document.getElementById('parseModal');
    const optionsModal = document.getElementById('optionsModal');
    const manualPasteModal = document.getElementById('manualPasteModal');

    const mediaThumb = document.getElementById('mediaThumb');
    const mediaTitle = document.getElementById('mediaTitle');
    const mediaChannelName = document.getElementById('mediaChannelName');
    const mediaTypeBadge = document.getElementById('mediaTypeBadge');

    const btnCloseOptions = document.getElementById('btnCloseOptions');
    const btnCancelOptions = document.getElementById('btnCancelOptions');
    const btnStartDownload = document.getElementById('btnStartDownload');
    const typeTabs = document.querySelectorAll('.type-tab');
    const formatGrid = document.getElementById('formatGrid');

    const fpsSection = document.getElementById('fpsSection');
    const subfolderInfoSection = document.getElementById('subfolderInfoSection');
    const subfolderNameText = document.getElementById('subfolderNameText');
    const orderSection = document.getElementById('orderSection');
    const videoOrderSelect = document.getElementById('videoOrderSelect');

    const batchSection = document.getElementById('batchSection');
    const batchList = document.getElementById('batchList');
    const batchCountText = document.getElementById('batchCountText');
    const btnSelectAll = document.getElementById('btnSelectAll');
    const btnSelectNone = document.getElementById('btnSelectNone');

    const manualUrlInput = document.getElementById('manualUrlInput');
    const btnCancelPaste = document.getElementById('btnCancelPaste');
    const btnSubmitPaste = document.getElementById('btnSubmitPaste');

    // Formats Config
    const videoFormats = [
        { format: '1080p', container: 'mp4', title: '1080p Full HD', sub: 'High Quality • MP4' },
        { format: '4k', container: 'mp4', title: '4K Ultra HD', sub: '2160p • MP4 / MKV' },
        { format: '8k', container: 'mkv', title: '8K Ultra HD', sub: '4320p • MKV' },
        { format: '720p', container: 'mp4', title: '720p HD', sub: 'Fast Download • MP4' },
        { format: '480p', container: 'mp4', title: '480p SD', sub: 'Standard • MP4' },
        { format: '360p', container: 'mp4', title: '360p SD', sub: 'Low Data • MP4' }
    ];

    const audioFormats = [
        { format: 'mp3', container: 'mp3', title: 'MP3 Audio (320kbps)', sub: 'Best Audio Quality' },
        { format: 'm4a', container: 'm4a', title: 'M4A Audio (AAC)', sub: 'High Quality AAC' },
        { format: 'flac', container: 'flac', title: 'FLAC Lossless', sub: 'Uncompressed Audio' },
        { format: 'wav', container: 'wav', title: 'WAV Audio', sub: 'PCM Uncompressed' },
        { format: 'ogg', container: 'ogg', title: 'OGG Vorbis', sub: 'Standard Vorbis' }
    ];

    renderFormatGrid();
    loadSettings();

    // Auto-poll tasks and history every 1 second
    setInterval(() => {
        pollTasks();
        fetchHistory();
    }, 1000);

    pollTasks();
    fetchHistory();

    // Navigation Tabs
    function switchTab(tabName) {
        [navDownloads, navHistory, navSettings].forEach(el => el && el.classList.remove('active'));
        [viewDownloads, viewHistory, viewSettings].forEach(el => el && (el.style.display = 'none'));

        if (tabName === 'downloads') {
            navDownloads.classList.add('active');
            viewDownloads.style.display = 'flex';
        } else if (tabName === 'history') {
            navHistory.classList.add('active');
            viewHistory.style.display = 'flex';
            fetchHistory();
        } else if (tabName === 'settings') {
            navSettings.classList.add('active');
            viewSettings.style.display = 'flex';
            loadSettings();
        }
    }

    if (navDownloads) navDownloads.addEventListener('click', (e) => { e.preventDefault(); switchTab('downloads'); });
    if (navHistory) navHistory.addEventListener('click', (e) => { e.preventDefault(); switchTab('history'); });
    if (navSettings) navSettings.addEventListener('click', (e) => { e.preventDefault(); switchTab('settings'); });

    // Toolbar & Global Actions
    if (btnPauseAll) {
        btnPauseAll.addEventListener('click', async () => {
            await fetch(`${API_BASE}/pause-all`, { method: 'POST' });
            pollTasks();
        });
    }

    if (btnResumeAll) {
        btnResumeAll.addEventListener('click', async () => {
            await fetch(`${API_BASE}/resume-all`, { method: 'POST' });
            pollTasks();
        });
    }

    if (btnToolbarLocation) {
        btnToolbarLocation.addEventListener('click', () => {
            switchTab('settings');
        });
    }

    // Event Listeners
    btnPasteLink.addEventListener('click', handlePasteLink);
    btnCancelPaste.addEventListener('click', () => manualPasteModal.classList.remove('active'));
    btnSubmitPaste.addEventListener('click', () => {
        const url = manualUrlInput.value.trim();
        if (url) {
            manualPasteModal.classList.remove('active');
            processUrl(url);
        }
    });

    btnCloseOptions.addEventListener('click', () => optionsModal.classList.remove('active'));
    btnCancelOptions.addEventListener('click', () => optionsModal.classList.remove('active'));

    btnOpenFolder.addEventListener('click', () => {
        fetch(`${API_BASE}/open-folder`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path: '' })
        });
    });

    btnClearCompleted.addEventListener('click', async () => {
        await fetch(`${API_BASE}/clear-completed`, { method: 'POST' });
        pollTasks();
    });

    if (btnClearHistory) {
        btnClearHistory.addEventListener('click', async () => {
            if (confirm("Are you sure you want to clear all download history?")) {
                await fetch(`${API_BASE}/history/clear`, { method: 'POST' });
                fetchHistory();
            }
        });
    }

    if (historySearchInput) {
        historySearchInput.addEventListener('input', (e) => {
            historySearchQuery = e.target.value.trim();
            renderHistory();
        });
    }

    historyFilterPills.forEach(pill => {
        pill.addEventListener('click', () => {
            historyFilterPills.forEach(p => p.classList.remove('active'));
            pill.classList.add('active');
            activeHistoryFilter = pill.dataset.filter;
            renderHistory();
        });
    });

    speedLimitSelect.addEventListener('change', (e) => {
        fetch(`${API_BASE}/settings`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ speed_limit: e.target.value })
        });
    });

    smartModeSwitch.addEventListener('change', (e) => {
        isSmartMode = e.target.checked;
        const tag = document.getElementById('smartModeTag');
        if (tag) {
            tag.textContent = isSmartMode ? 'ON' : 'OFF';
            tag.classList.toggle('active', isSmartMode);
        }
    });

    const fpsPills = document.querySelectorAll('#fpsSection .pill-btn');
    fpsPills.forEach(pill => {
        pill.addEventListener('click', () => {
            fpsPills.forEach(p => p.classList.remove('active'));
            pill.classList.add('active');
            selectedFps = pill.dataset.fps;
        });
    });

    typeTabs.forEach(tab => {
        tab.addEventListener('click', () => {
            typeTabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            selectedType = tab.dataset.type;
            fpsSection.style.display = selectedType === 'video' ? 'block' : 'none';
            renderFormatGrid();
        });
    });

    btnSelectAll.addEventListener('click', () => {
        if (!currentParsedData || !currentParsedData.items) return;
        selectedItemIndices = new Set(currentParsedData.items.map((_, i) => i));
        updateBatchUI();
    });

    btnSelectNone.addEventListener('click', () => {
        selectedItemIndices.clear();
        updateBatchUI();
    });

    btnSaveOutputDir.addEventListener('click', async () => {
        const newDir = settingsOutputDir.value.trim();
        if (newDir) {
            await fetch(`${API_BASE}/settings`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ output_dir: newDir })
            });
            loadSettings();
            alert("Destination directory saved!");
        }
    });

    settingsSubfolderCheck.addEventListener('change', (e) => {
        isSubfolderEnabled = e.target.checked;
    });

    settingsMaxConcurrent.addEventListener('change', (e) => {
        fetch(`${API_BASE}/settings`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ max_concurrent: e.target.value })
        });
    });

    btnStartDownload.addEventListener('click', startDownload);

    async function loadSettings() {
        try {
            const res = await fetch(`${API_BASE}/settings`);
            const data = await res.json();
            if (data.output_dir) {
                settingsOutputDir.value = data.output_dir;
                const displayPath = data.output_dir.length > 28 ? '...' + data.output_dir.slice(-25) : data.output_dir;
                if (toolbarLocationText) toolbarLocationText.textContent = displayPath;
                if (statHistoryLocation) statHistoryLocation.textContent = displayPath;
            }
            if (data.max_concurrent) settingsMaxConcurrent.value = data.max_concurrent;
            if (data.speed_limit) speedLimitSelect.value = data.speed_limit || 'unlimited';
        } catch (e) {
            console.log("Could not load settings");
        }
    }

    function renderFormatGrid() {
        const items = selectedType === 'video' ? videoFormats : audioFormats;
        formatGrid.innerHTML = items.map((item, idx) => `
            <div class="format-card ${idx === 0 ? 'selected' : ''}" data-format="${item.format}">
                <div class="format-res">${item.title}</div>
                <div class="format-sub">${item.sub}</div>
            </div>
        `).join('');

        if (selectedType === 'video') {
            selectedFormat = items[0].format;
        } else {
            selectedAudioFormat = items[0].format;
        }

        const cards = formatGrid.querySelectorAll('.format-card');
        cards.forEach(card => {
            card.addEventListener('click', () => {
                cards.forEach(c => c.classList.remove('selected'));
                card.classList.add('selected');
                if (selectedType === 'video') {
                    selectedFormat = card.dataset.format;
                } else {
                    selectedAudioFormat = card.dataset.format;
                }
            });
        });
    }

    async function handlePasteLink() {
        try {
            const text = await navigator.clipboard.readText();
            if (text && (text.includes('youtube.com') || text.includes('youtu.be'))) {
                processUrl(text.trim());
                return;
            }
        } catch (e) {
            console.log("Clipboard direct access unavailable, showing manual prompt.");
        }
        manualUrlInput.value = '';
        manualPasteModal.classList.add('active');
    }

    async function processUrl(url) {
        parseModal.classList.add('active');
        try {
            const res = await fetch(`${API_BASE}/parse`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ url: url })
            });
            const data = await res.json();
            parseModal.classList.remove('active');

            if (!data.success && (!data.items || data.items.length === 0)) {
                alert("Failed to extract YouTube link metadata: " + (data.error_message || "Unknown error"));
                return;
            }

            currentParsedData = data;

            if (isSmartMode) {
                enqueueSmartDownload(data);
            } else {
                showOptionsModal(data);
            }
        } catch (err) {
            parseModal.classList.remove('active');
            alert("Error connecting to C++ Core Backend: " + err.message);
        }
    }

    function showOptionsModal(data) {
        mediaThumb.src = data.thumbnail_url || 'https://via.placeholder.com/120x68';
        mediaTitle.textContent = data.title || 'YouTube Content';
        mediaChannelName.textContent = data.channel_name || 'YouTube Creator';
        mediaTypeBadge.textContent = data.type === 'channel' ? `Channel (${data.total_items} items)` :
                                     data.type === 'playlist' ? `Playlist (${data.total_items} items)` : 'Single Video';

        if (isSubfolderEnabled && (data.type === 'channel' || data.type === 'playlist')) {
            subfolderInfoSection.style.display = 'block';
            subfolderNameText.textContent = data.title || data.channel_name;
            orderSection.style.display = 'block';
        } else {
            subfolderInfoSection.style.display = 'none';
            orderSection.style.display = 'none';
        }

        if (data.items && data.items.length > 1) {
            batchSection.style.display = 'block';
            selectedItemIndices = new Set(data.items.map((_, i) => i));
            renderBatchList(data.items);
        } else {
            batchSection.style.display = 'none';
            selectedItemIndices = new Set([0]);
        }

        optionsModal.classList.add('active');
    }

    function renderBatchList(items) {
        batchList.innerHTML = items.map((item, idx) => `
            <div class="batch-item" style="display:flex; align-items:center; gap:12px; padding:8px; border-bottom:1px solid rgba(255,255,255,0.05);">
                <input type="checkbox" class="batch-checkbox" data-idx="${idx}" ${selectedItemIndices.has(idx) ? 'checked' : ''}>
                <img src="${item.thumbnail || currentParsedData.thumbnail_url}" style="width:60px; height:34px; border-radius:4px; object-fit:cover;">
                <div style="flex:1; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; font-size:13px;">${item.title}</div>
            </div>
        `).join('');

        updateBatchUI();

        const checkboxes = batchList.querySelectorAll('.batch-checkbox');
        checkboxes.forEach(cb => {
            cb.addEventListener('change', (e) => {
                const idx = parseInt(e.target.dataset.idx, 10);
                if (e.target.checked) {
                    selectedItemIndices.add(idx);
                } else {
                    selectedItemIndices.delete(idx);
                }
                updateBatchUI();
            });
        });
    }

    function updateBatchUI() {
        if (!currentParsedData || !currentParsedData.items) return;
        batchCountText.textContent = `Selected ${selectedItemIndices.size} of ${currentParsedData.items.length} Videos`;
        const checkboxes = batchList.querySelectorAll('.batch-checkbox');
        checkboxes.forEach(cb => {
            const idx = parseInt(cb.dataset.idx, 10);
            cb.checked = selectedItemIndices.has(idx);
        });
    }

    async function startDownload() {
        if (!currentParsedData) return;

        optionsModal.classList.remove('active');

        const subCheck = document.getElementById('subCheck').checked;
        const subLangSelect = document.getElementById('subLangSelect').value;

        const allItems = currentParsedData.items && currentParsedData.items.length > 0 ?
                         currentParsedData.items : [{ url: currentParsedData.url, title: currentParsedData.title, thumbnail: currentParsedData.thumbnail_url }];

        let selectedItems = allItems.filter((_, idx) => selectedItemIndices.has(idx));

        const orderMode = videoOrderSelect.value || 'newest';
        if (orderMode === 'oldest') {
            selectedItems.reverse();
        }

        const baseSubfolder = (isSubfolderEnabled && (currentParsedData.type === 'channel' || currentParsedData.type === 'playlist')) ?
                             (currentParsedData.title || currentParsedData.channel_name) : '';

        for (let i = 0; i < selectedItems.length; i++) {
            const item = selectedItems[i];

            let subfolderPath = baseSubfolder;
            if (baseSubfolder && currentParsedData.type === 'channel') {
                const isShort = item.url.includes('/shorts/') || (item.title && item.title.toLowerCase().includes('#shorts'));
                subfolderPath = isShort ? `${baseSubfolder}/Shorts` : `${baseSubfolder}/Videos`;
            }

            const numPrefix = (selectedItems.length > 1) ? `${String(i + 1).padStart(2, '0')}_` : '';
            const finalTitle = numPrefix ? `${numPrefix}${item.title}` : item.title;

            await fetch(`${API_BASE}/download`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    url: item.url,
                    title: finalTitle,
                    thumbnail: item.thumbnail,
                    type: selectedType,
                    format: selectedType === 'video' ? selectedFormat : selectedAudioFormat,
                    fps: selectedFps,
                    audio_format: selectedAudioFormat,
                    audio_quality: '320',
                    video_format: settingsFormatSelect.value || 'mp4',
                    subtitles: subCheck,
                    sub_lang: subLangSelect,
                    subfolder: subfolderPath
                })
            });
        }

        switchTab('downloads');
        pollTasks();
    }

    async function enqueueSmartDownload(data) {
        const itemsToDownload = data.items && data.items.length > 0 ? data.items : [{ url: data.url, title: data.title, thumbnail: data.thumbnail_url }];
        const baseSubfolder = (isSubfolderEnabled && (data.type === 'channel' || data.type === 'playlist')) ? (data.title || data.channel_name) : '';

        for (let i = 0; i < itemsToDownload.length; i++) {
            const item = itemsToDownload[i];
            let subfolderPath = baseSubfolder;
            if (baseSubfolder && data.type === 'channel') {
                const isShort = item.url.includes('/shorts/') || (item.title && item.title.toLowerCase().includes('#shorts'));
                subfolderPath = isShort ? `${baseSubfolder}/Shorts` : `${baseSubfolder}/Videos`;
            }

            const numPrefix = (itemsToDownload.length > 1) ? `${String(i + 1).padStart(2, '0')}_` : '';
            const finalTitle = numPrefix ? `${numPrefix}${item.title}` : item.title;

            await fetch(`${API_BASE}/download`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    url: item.url,
                    title: finalTitle,
                    thumbnail: item.thumbnail,
                    type: 'video',
                    format: '1080p',
                    fps: '60',
                    video_format: 'mp4',
                    subfolder: subfolderPath
                })
            });
        }
        switchTab('downloads');
        pollTasks();
    }

    // Poll Active Tasks
    async function pollTasks() {
        try {
            const res = await fetch(`${API_BASE}/tasks`);
            const tasks = await res.json();
            renderTasks(tasks);
        } catch (e) {
            // Server active
        }
    }

    function renderTasks(tasks) {
        const activeTasks = tasks.filter(t => t.status === 'downloading' || t.status === 'queued');
        activeCountBadge.textContent = activeTasks.length;
        queueTotalText.textContent = `${tasks.length} Tasks`;

        if (!tasks || tasks.length === 0) {
            emptyState.style.display = 'flex';
            downloadList.innerHTML = '';
            totalSpeedText.textContent = '0 KB/s';
            return;
        }

        emptyState.style.display = 'none';

        downloadList.innerHTML = tasks.map(t => {
            const isCompleted = t.status === 'completed';
            const isDownloading = t.status === 'downloading';
            const isError = t.status === 'error';
            const isPaused = t.status === 'paused';

            return `
                <div class="task-card">
                    <img class="task-thumb" src="${t.thumbnail || 'https://via.placeholder.com/100x56'}" alt="Thumb" onerror="this.src='https://via.placeholder.com/100x56'">
                    <div class="task-info">
                        <div class="task-title" title="${t.title}">${t.title || t.url}</div>
                        <div class="task-meta">
                            <span class="status-badge ${t.status}">${t.status}</span>
                            <span>${t.progress.toFixed(1)}%</span>
                            ${isDownloading ? `<span>${t.speed}</span> • <span>ETA ${t.eta}</span>` : ''}
                            ${isCompleted ? `<span style="color:var(--accent-green);"><i class="fa-solid fa-circle-check"></i> Finished</span>` : ''}
                            ${isError ? `<span style="color:var(--danger-red);">${t.error || 'Failed'}</span>` : ''}
                        </div>
                        ${t.filepath ? `
                        <div class="task-location-path" title="${t.filepath}">
                            <i class="fa-solid fa-folder"></i> ${t.filepath}
                        </div>` : ''}
                        <div class="progress-bar-container">
                            <div class="progress-bar-fill" style="width: ${t.progress}%;"></div>
                        </div>
                    </div>
                    <div class="task-actions">
                        ${isDownloading ? `<button class="btn btn-icon" onclick="pauseTask('${t.id}')" title="Pause Download"><i class="fa-solid fa-pause"></i></button>` : ''}
                        ${isPaused ? `<button class="btn btn-icon" onclick="resumeTask('${t.id}')" title="Resume Download"><i class="fa-solid fa-play"></i></button>` : ''}
                        ${isError ? `<button class="btn btn-icon" onclick="resumeTask('${t.id}')" title="Retry Download"><i class="fa-solid fa-rotate-right"></i></button>` : ''}
                        ${t.filepath ? `<button class="btn btn-icon" onclick="openFileFolder('${t.filepath.replace(/\\/g, '\\\\')}')" title="Show File Location"><i class="fa-solid fa-folder-open"></i></button>` : ''}
                        <button class="btn btn-icon" onclick="removeTask('${t.id}')" title="Remove Task"><i class="fa-solid fa-trash"></i></button>
                    </div>
                </div>
            `;
        }).join('');
    }

    // Persistent History Fetch & Render
    async function fetchHistory() {
        try {
            const res = await fetch(`${API_BASE}/history`);
            const data = await res.json();
            historyData = data || [];
            if (historyCountBadge) historyCountBadge.textContent = historyData.length;
            renderHistory();
        } catch (e) {
            console.log("Could not load history");
        }
    }

    function renderHistory() {
        if (!historyList) return;

        let items = historyData;

        if (activeHistoryFilter === 'video') {
            items = items.filter(h => h.type === 'video');
        } else if (activeHistoryFilter === 'audio') {
            items = items.filter(h => h.type === 'audio');
        }

        if (historySearchQuery) {
            const q = historySearchQuery.toLowerCase();
            items = items.filter(h => (h.title && h.title.toLowerCase().includes(q)) || (h.url && h.url.toLowerCase().includes(q)) || (h.format && h.format.toLowerCase().includes(q)));
        }

        const completedItems = historyData.filter(h => h.status === 'completed');
        if (statCompletedCount) statCompletedCount.textContent = completedItems.length;

        const totalBytes = completedItems.reduce((acc, curr) => acc + (curr.filesize || 0), 0);
        if (statTotalSize) statTotalSize.textContent = formatBytes(totalBytes);

        const historyCountText = document.getElementById('historyCountText');
        if (historyCountText) historyCountText.textContent = `${items.length} Items`;

        if (items.length === 0) {
            if (historyEmptyState) historyEmptyState.style.display = 'flex';
            historyList.innerHTML = '';
            return;
        }

        if (historyEmptyState) historyEmptyState.style.display = 'none';

        historyList.innerHTML = items.map(h => {
            const formattedSize = h.filesize ? formatBytes(h.filesize) : '';

            return `
                <div class="task-card">
                    <img class="task-thumb" src="${h.thumbnail || 'https://via.placeholder.com/100x56'}" alt="Thumb" onerror="this.src='https://via.placeholder.com/100x56'">
                    <div class="task-info">
                        <div class="task-title" title="${h.title}">${h.title || h.url}</div>
                        <div class="task-meta">
                            <span class="status-badge ${h.status}">${h.status}</span>
                            <span class="tag-badge" style="background:rgba(255,255,255,0.08); padding:2px 8px; border-radius:4px; font-size:11px;">${h.format || h.type}</span>
                            ${formattedSize ? `<span>${formattedSize}</span>` : ''}
                            <span>• ${h.timestamp || ''}</span>
                        </div>
                        ${h.filepath ? `
                        <div class="task-location-path" title="${h.filepath}">
                            <i class="fa-solid fa-folder"></i> ${h.filepath}
                        </div>` : ''}
                    </div>
                    <div class="task-actions">
                        ${h.filepath ? `<button class="btn btn-icon" onclick="openFileFolder('${h.filepath.replace(/\\/g, '\\\\')}')" title="Open File / Folder"><i class="fa-solid fa-folder-open"></i></button>` : ''}
                        <button class="btn btn-icon" onclick="redownloadUrl('${h.url.replace(/'/g, "\\'")}')" title="Re-download Video"><i class="fa-solid fa-rotate-right"></i></button>
                        <button class="btn btn-icon" onclick="removeHistoryItem('${h.id}')" title="Delete Entry"><i class="fa-solid fa-trash"></i></button>
                    </div>
                </div>
            `;
        }).join('');
    }

    function formatBytes(bytes) {
        if (!bytes || bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
    }

    // Global Action Helpers
    window.pauseTask = (id) => fetch(`${API_BASE}/pause`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id }) }).then(pollTasks);
    window.resumeTask = (id) => fetch(`${API_BASE}/resume`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id }) }).then(pollTasks);
    window.removeTask = (id) => fetch(`${API_BASE}/remove`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id }) }).then(pollTasks);
    window.openFileFolder = (filepath) => fetch(`${API_BASE}/open-folder`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ path: filepath }) });
    window.removeHistoryItem = (id) => fetch(`${API_BASE}/history/remove`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id }) }).then(fetchHistory);
    window.redownloadUrl = (url) => processUrl(url);
});
