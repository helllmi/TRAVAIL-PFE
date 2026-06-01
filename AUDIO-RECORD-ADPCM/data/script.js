let polling = null;

async function startRecording() {
  document.getElementById('recBtn').disabled = true;
  document.getElementById('dlLink').style.display = 'none';
  setStatus('Enregistrement en cours...', 'recording');
  await fetch('/record');
  polling = setInterval(checkStatus, 1000);
}

async function checkStatus() {
  try {
    const r = await fetch('/status');
    const d = await r.json();
    document.getElementById('bar').style.width = d.progress + '%';
    if (d.error) {
      clearInterval(polling);
      setStatus('Erreur : ' + d.error, 'error');
      document.getElementById('recBtn').disabled = false;
    } else if (d.done) {
      clearInterval(polling);
      setStatus('Terminé ✓  (' + d.size + ' bytes)', 'done');
      document.getElementById('dlLink').style.display = 'inline-block';
      document.getElementById('recBtn').disabled = false;
    } else {
      setStatus('Enregistrement... ' + d.progress + '%', 'recording');
    }
  } catch(e) {}
}

function setStatus(msg, cls) {
  const el = document.getElementById('status');
  el.textContent = msg;
  el.className = 'status ' + (cls || '');
}
