document.addEventListener('DOMContentLoaded', function () {
  const ws = new WebSocket('ws://' + window.location.hostname + '/ws');

  ws.onmessage = function (event) {
    const data = JSON.parse(event.data);
    updateSensorValues(data);
  };

  ws.onopen = function () {
    console.log('WebSocket connection established');
  };

  ws.onclose = function () {
    console.log('WebSocket connection closed');
  };

  function updateSensorValues(data) {
    document.getElementById('temperature').textContent = data.temperature.toFixed(2);
    document.getElementById('pression').textContent = data.pression.toFixed(2) ;
    document.getElementById('gasValue').textContent = data.gasValue;
    document.getElementById('lux').textContent = data.lux.toFixed(2);
    document.getElementById('photo_res').textContent = data.photo_res.toFixed(2);
    updateButton(data.buttonState);
  }

  function updateButton(buttonState) {
    const button = document.getElementById('toggle-button');
    button.className = buttonState ? 'green' : 'red';
    button.textContent = buttonState ? 'Activé' : 'Désactivé';
  }

  document.getElementById('toggle-button').addEventListener('click', function () {
    ws.send(JSON.stringify({ action: "toggleButton" }));
  });

  document.getElementById('frequency-slider').addEventListener('input', function (e) {
    // Send the new frequency value as an integer
    ws.send(JSON.stringify({ action: "setFrequency", value: parseInt(e.target.value, 10) }));
    
    // Update the display of the frequency value on the webpage
    document.getElementById('frequency-value').textContent = e.target.value;
  });
});
