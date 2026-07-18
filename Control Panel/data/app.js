// ========================================
// STATE
// ========================================

let inputs = [];
let editingIndex = null;

// ========================================
// DOMAIN -> SERVICE LOOKUP
// ========================================

const DOMAIN_SERVICES = {
  light:         ['turn_on', 'turn_off', 'toggle'],
  switch:        ['turn_on', 'turn_off', 'toggle'],
  fan:           ['turn_on', 'turn_off', 'toggle'],
  cover:         ['open_cover', 'close_cover', 'toggle'],
  lock:          ['lock', 'unlock'],
  script:        ['turn_on'],
  scene:         ['turn_on'],
  automation:    ['trigger', 'turn_on', 'turn_off'],
  input_boolean: ['turn_on', 'turn_off', 'toggle'],
  media_player:  ['turn_on', 'turn_off', 'toggle', 'media_play_pause', 'volume_up', 'volume_down'],
  vacuum:        ['start', 'stop', 'return_to_base'],
  climate:       ['turn_on', 'turn_off'],
};
const DEFAULT_SERVICES = ['turn_on', 'turn_off', 'toggle'];
const CUSTOM_VALUE = '__custom__';

const TYPE_ICON = { 0: '\u25CF', 1: '\u25C6' }; // filled circle = button, diamond = toggle

// ========================================
// DOM REFERENCES
// ========================================

const modalBackdrop = document.getElementById('modal-backdrop');
const modalTitle = document.getElementById('modal-title');
const modalError = document.getElementById('modal-error');
const inputEntity = document.getElementById('input-entity');
const inputLabel = document.getElementById('input-label');
const inputService = document.getElementById('input-service');

const customServiceWrapper = document.getElementById('custom-service-wrapper');
const inputServiceCustom = document.getElementById('input-service-custom');
const inputType = document.getElementById('input-type');
const btnSave = document.getElementById('btn-save');
const btnCancel = document.getElementById('btn-cancel');
const wifiDot = document.getElementById('wifi-dot');
const wifiText = document.getElementById('wifi-text');

// ========================================
// DOMAIN-AWARE SERVICE DROPDOWN
// ========================================

function getDomain(entityId) {
  const dot = entityId.indexOf('.');
  return dot > 0 ? entityId.substring(0, dot) : '';
}

function populateServiceOptions(domain, selectedService) {
  const services = DOMAIN_SERVICES[domain] || DEFAULT_SERVICES;

  inputService.innerHTML = '';
  services.forEach(svc => {
    const opt = document.createElement('option');
    opt.value = svc;
    opt.textContent = svc;
    inputService.appendChild(opt);
  });
  const customOpt = document.createElement('option');
  customOpt.value = CUSTOM_VALUE;
  customOpt.textContent = 'Other (custom)...';
  inputService.appendChild(customOpt);

  if (selectedService && services.includes(selectedService)) {
    inputService.value = selectedService;
    customServiceWrapper.classList.add('hidden');
    inputServiceCustom.value = '';
  } else if (selectedService) {
    // Existing service not in the known list for this domain - use custom slot
    inputService.value = CUSTOM_VALUE;
    customServiceWrapper.classList.remove('hidden');
    inputServiceCustom.value = selectedService;
  } else {
    inputService.value = services[0];
    customServiceWrapper.classList.add('hidden');
    inputServiceCustom.value = '';
  }
}

function refreshServiceOptionsFromEntity(preserveSelection) {
  const domain = getDomain(inputEntity.value.trim());
  const currentSelected = preserveSelection
    ? (inputService.value === CUSTOM_VALUE ? inputServiceCustom.value.trim() : inputService.value)
    : null;
  populateServiceOptions(domain, currentSelected);
}

inputEntity.addEventListener('input', () => refreshServiceOptionsFromEntity(true));

inputService.addEventListener('change', () => {
  if (inputService.value === CUSTOM_VALUE) {
    customServiceWrapper.classList.remove('hidden');
  } else {
    customServiceWrapper.classList.add('hidden');
    inputServiceCustom.value = '';
  }
});

// ========================================
// RENDERING
// ========================================

function renderTile(input) {
  const tile = document.getElementById('tile-' + input.index);
  if (!tile) return;

  tile.innerHTML = '';
  tile.classList.remove('state-on');

  const icon = document.createElement('div');
  icon.className = 'tile-type-icon';
  icon.textContent = TYPE_ICON[input.type] || '';
  tile.appendChild(icon);

  if (input.type === 1) {
    if (input.currentState === true) {
      tile.classList.add('state-on');
    }
    const indicator = document.createElement('div');
    indicator.className = 'toggle-indicator';
    tile.appendChild(indicator);
  }

  const name = document.createElement('div');
  name.className = 'tile-name';
  const displayName = input.label || input.name;
  name.textContent = displayName;
  name.title = input.label ? input.name + ': ' + input.label : input.name;
  tile.appendChild(name);

  const detail = document.createElement('div');
  detail.className = 'tile-detail';
  const detailText = input.entityId || '(unassigned)';
  detail.textContent = detailText;
  detail.title = detailText;
  tile.appendChild(detail);



  tile.onclick = () => openEditModal(input.index);
}

function renderAll() {
  inputs.forEach(renderTile);
}

// ========================================
// API CALLS
// ========================================

function fetchInputs() {
  fetch('/api/inputs')
    .then(res => res.json())
    .then(data => {
      inputs = data;
      renderAll();
    })
    .catch(err => console.error('Failed to fetch inputs:', err));
}

function fetchStatus() {
  fetch('/api/status')
    .then(res => res.json())
    .then(data => {
      if (data.wifiConnected) {
        wifiDot.className = 'dot online';
        wifiText.textContent = data.ip || 'Connected';
      } else {
        wifiDot.className = 'dot offline';
        wifiText.textContent = 'Offline';
      }
    })
    .catch(() => {
      wifiDot.className = 'dot offline';
      wifiText.textContent = 'Unreachable';
    });
}

function saveInput(index, entityId, service, type, label) {
  return fetch('/api/input', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ index, entityId, service, type, label })
  }).then(res => {

    if (!res.ok) {
      return res.json().then(err => { throw new Error(err.error || 'Save failed'); });
    }
    return res.json();
  });
}

// ========================================
// MODAL
// ========================================

function openEditModal(index) {
  const input = inputs.find(i => i.index === index);
  if (!input) return;

  editingIndex = index;
  modalTitle.textContent = 'Edit: ' + input.name;
  inputEntity.value = input.entityId || '';
  inputLabel.value = input.label || '';
  inputType.value = String(input.type);


  const domain = getDomain(inputEntity.value.trim());
  populateServiceOptions(domain, input.service || '');

  modalError.classList.add('hidden');
  modalError.textContent = '';
  modalBackdrop.classList.remove('hidden');
}

function closeModal() {
  modalBackdrop.classList.add('hidden');
  editingIndex = null;
}

btnCancel.addEventListener('click', closeModal);

btnSave.addEventListener('click', () => {
  if (editingIndex === null) return;

  const entityId = inputEntity.value.trim();
  const label = inputLabel.value.trim();
  const service = inputService.value === CUSTOM_VALUE
    ? inputServiceCustom.value.trim()
    : inputService.value;
  const type = parseInt(inputType.value, 10);

  if (!entityId || !service) {
    modalError.textContent = 'Entity ID and Service are required.';
    modalError.classList.remove('hidden');
    return;
  }

  btnSave.disabled = true;
  saveInput(editingIndex, entityId, service, type, label)
    .then(() => {
      const input = inputs.find(i => i.index === editingIndex);
      if (input) {
        input.entityId = entityId;
        input.service = service;
        input.type = type;
        input.label = label;
        renderTile(input);
      }
      closeModal();
    })

    .catch(err => {
      modalError.textContent = err.message || 'Save failed.';
      modalError.classList.remove('hidden');
    })
    .finally(() => {
      btnSave.disabled = false;
    });
});

modalBackdrop.addEventListener('click', (e) => {
  if (e.target === modalBackdrop) closeModal();
});

// ========================================
// INIT
// ========================================

fetchInputs();
fetchStatus();
setInterval(fetchStatus, 5000);
setInterval(fetchInputs, 10000);
