// CNC Stop Block — WebSocket Client & Touch UI Controller

(function() {
    'use strict';

    // --- State ---
    let ws = null;
    let reconnectDelay = 1000;
    let jogStep = 0.1;
    let lastState = {};

    // --- DOM References ---
    const $ = (sel) => document.querySelector(sel);
    const $$ = (sel) => document.querySelectorAll(sel);

    const dom = {
        stateBadge:   $('#state-badge'),
        toolInfo:     $('#tool-info'),
        droValue:     $('#dro-value'),
        targetValue:  $('#target-value'),
        errorValue:   $('#error-value'),
        gotoInput:    $('#goto-input'),
        cutlistItems: $('#cutlist-items'),
        errorBanner:  $('#error-banner'),
        errorMessage: $('#error-message'),
        addModal:     $('#add-cut-modal'),
        btnLock:      $('#btn-lock'),
        btnCut:       $('#btn-cut'),
        btnReset:     $('#btn-reset'),
        btnEstop:     $('#btn-estop'),
    };

    // --- WebSocket ---

    function connect() {
        const host = window.location.hostname || '192.168.4.1';
        const url = 'ws://' + host + '/ws';
        ws = new WebSocket(url);

        ws.onopen = function() {
            console.log('[WS] Connected');
            reconnectDelay = 1000;
            $('#wifi-status').textContent = 'Connected';
        };

        ws.onclose = function() {
            console.log('[WS] Disconnected, retrying in ' + reconnectDelay + 'ms');
            $('#wifi-status').textContent = 'Disconnected';
            setTimeout(connect, reconnectDelay);
            reconnectDelay = Math.min(reconnectDelay * 2, 10000);
        };

        ws.onerror = function() {
            ws.close();
        };

        ws.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                updateUI(data);
                lastState = data;
            } catch (e) {
                console.error('[WS] Parse error:', e);
            }
        };
    }

    function send(obj) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(obj));
        }
    }

    // --- UI Updates ---

    function updateUI(data) {
        // State badge
        const state = data.state || 'UNKNOWN';
        dom.stateBadge.textContent = state;
        dom.stateBadge.className = 'badge ' + state.toLowerCase();

        // DRO
        dom.droValue.textContent = parseFloat(data.pos_mm).toFixed(2);
        dom.targetValue.textContent = parseFloat(data.target_mm).toFixed(2);

        const error = Math.abs(parseFloat(data.pos_mm) - parseFloat(data.target_mm));
        dom.errorValue.textContent = error.toFixed(2);

        // Tool info
        if (data.tool) {
            dom.toolInfo.textContent = data.tool.name + ' (kerf: ' + data.tool.kerf_mm + 'mm)';
        } else {
            dom.toolInfo.textContent = 'No tool detected';
        }

        // Indicators
        toggleIndicator('ind-homed', data.homed);
        toggleIndicator('ind-locked', data.locked);
        toggleIndicator('ind-dust', data.dust);
        toggleIndicator('ind-clamps', data.clamps);

        // Lock button state
        if (data.locked) {
            dom.btnLock.textContent = 'UNLOCK';
            dom.btnLock.classList.add('locked');
        } else {
            dom.btnLock.textContent = 'LOCK';
            dom.btnLock.classList.remove('locked');
        }

        // Cut button state
        if (state === 'CUTTING') {
            dom.btnCut.textContent = 'END CUT';
            dom.btnCut.classList.add('cutting');
        } else {
            dom.btnCut.textContent = 'START CUT';
            dom.btnCut.classList.remove('cutting');
        }

        // Error/E-Stop
        if (state === 'ERROR' || state === 'ESTOP') {
            dom.errorBanner.style.display = 'flex';
            dom.errorMessage.textContent = data.error || state;
            dom.btnReset.style.display = '';
            dom.btnEstop.style.display = 'none';
        } else {
            dom.errorBanner.style.display = 'none';
            dom.btnReset.style.display = 'none';
            dom.btnEstop.style.display = '';
        }

        // Cut list (only refresh if index changed)
        if (data.cutlist_idx !== lastState.cutlist_idx ||
            data.cutlist_size !== lastState.cutlist_size) {
            fetchCutList(data.cutlist_idx);
        }
    }

    function toggleIndicator(id, active) {
        const el = $('#' + id);
        if (active) {
            el.classList.add('active');
        } else {
            el.classList.remove('active');
        }
    }

    // --- Cut List ---

    function fetchCutList(activeIdx) {
        fetch('/api/cutlist')
            .then(r => r.json())
            .then(items => renderCutList(items, activeIdx))
            .catch(e => console.error('[CutList] Fetch error:', e));
    }

    function renderCutList(items, activeIdx) {
        dom.cutlistItems.innerHTML = '';

        items.forEach(function(item, i) {
            const div = document.createElement('div');
            div.className = 'cut-item';
            if (i === activeIdx) div.classList.add('active');
            if (item.completed) div.classList.add('completed');

            div.innerHTML =
                '<span class="cut-label">' + escapeHtml(item.label || ('Cut ' + (i + 1))) + '</span>' +
                '<span class="cut-length">' + parseFloat(item.length_mm).toFixed(1) + '</span>' +
                '<span class="cut-qty">x' + (item.quantity || 1) + '</span>' +
                '<button class="cut-go" data-pos="' + item.length_mm + '">GO</button>';

            div.querySelector('.cut-go').addEventListener('click', function() {
                send({ cmd: 'goto', position_mm: parseFloat(this.dataset.pos) });
            });

            dom.cutlistItems.appendChild(div);
        });
    }

    function escapeHtml(str) {
        const div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }

    // --- Event Handlers ---

    function setupEventListeners() {
        // Control buttons
        $('#btn-home').addEventListener('click', function() {
            send({ cmd: 'home' });
        });

        dom.btnLock.addEventListener('click', function() {
            if (lastState.locked) {
                send({ cmd: 'unlock' });
            } else {
                send({ cmd: 'lock' });
            }
        });

        $('#btn-next').addEventListener('click', function() {
            send({ cmd: 'next_cut' });
        });

        dom.btnCut.addEventListener('click', function() {
            if (lastState.state === 'CUTTING') {
                send({ cmd: 'cut_end' });
            } else {
                send({ cmd: 'cut_start' });
            }
        });

        dom.btnEstop.addEventListener('click', function() {
            send({ cmd: 'estop' });
        });

        dom.btnReset.addEventListener('click', function() {
            send({ cmd: 'reset' });
        });

        $('#error-dismiss').addEventListener('click', function() {
            send({ cmd: 'reset' });
        });

        // Jog step selection
        $$('.jog-step').forEach(function(btn) {
            btn.addEventListener('click', function() {
                $$('.jog-step').forEach(function(b) { b.classList.remove('active'); });
                btn.classList.add('active');
                jogStep = parseFloat(btn.dataset.step);
            });
        });

        // Jog buttons
        $('#jog-minus').addEventListener('click', function() {
            send({ cmd: 'jog', distance_mm: -jogStep });
        });

        $('#jog-plus').addEventListener('click', function() {
            send({ cmd: 'jog', distance_mm: jogStep });
        });

        // Go To
        $('#goto-btn').addEventListener('click', function() {
            const val = parseFloat(dom.gotoInput.value);
            if (!isNaN(val) && val >= 0) {
                send({ cmd: 'goto', position_mm: val });
                dom.gotoInput.value = '';
                dom.gotoInput.blur();
            }
        });

        dom.gotoInput.addEventListener('keydown', function(e) {
            if (e.key === 'Enter') {
                $('#goto-btn').click();
            }
        });

        // Cut list management
        $('#cutlist-add').addEventListener('click', function() {
            dom.addModal.style.display = 'flex';
            $('#cut-label').value = '';
            $('#cut-length').value = '';
            $('#cut-qty').value = '1';
            setTimeout(function() { $('#cut-label').focus(); }, 100);
        });

        $('#cut-cancel').addEventListener('click', function() {
            dom.addModal.style.display = 'none';
        });

        $('#cut-save').addEventListener('click', function() {
            const label = $('#cut-label').value.trim();
            const length = parseFloat($('#cut-length').value);
            const qty = parseInt($('#cut-qty').value) || 1;

            if (!isNaN(length) && length > 0) {
                // Fetch current list, append, and send update
                fetch('/api/cutlist')
                    .then(function(r) { return r.json(); })
                    .then(function(items) {
                        items.push({
                            label: label || ('Cut ' + (items.length + 1)),
                            length_mm: length,
                            quantity: qty,
                            completed: false
                        });
                        send({ cmd: 'cutlist_update', data: JSON.stringify(items) });
                        dom.addModal.style.display = 'none';
                        setTimeout(function() { fetchCutList(lastState.cutlist_idx); }, 200);
                    });
            }
        });

        $('#cutlist-clear').addEventListener('click', function() {
            if (confirm('Clear all cuts?')) {
                send({ cmd: 'cutlist_clear' });
                setTimeout(function() { fetchCutList(-1); }, 200);
            }
        });

        $('#cutlist-reset').addEventListener('click', function() {
            send({ cmd: 'cutlist_reset' });
            setTimeout(function() { fetchCutList(0); }, 200);
        });

        // Close modal on backdrop click
        dom.addModal.addEventListener('click', function(e) {
            if (e.target === dom.addModal) {
                dom.addModal.style.display = 'none';
            }
        });
    }

    // --- Init ---
    document.addEventListener('DOMContentLoaded', function() {
        setupEventListeners();
        connect();
        // Initial cut list fetch
        fetchCutList(-1);
    });

})();
