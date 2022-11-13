const char HTML_MAIN[] PROGMEM = R"rawliteral(
<figure class="text-center">
    <h2 id="devicename"></h2>
</figure>
<div class="row gx-0 mb-2">
    <div class="col">
        <div class="bg-light">Today Ingoing:</div>
    </div>
    <div class="col">
        <div class="bg-light"><span id="amountin"></div>
    </div>
</div>
<div class="row gx-0 mb-2">
    <div class="col">
        <div class="bg-light">Today outgoing: </div>
    </div>
    <div class="col">
        <div class="bg-light"><span id="amountout"></span></div>
    </div>
</div>
<div class="row gx-0 mb-2">
    <div class="col">
        <div class="bg-light">Present at Shop: </div>
    </div>
    <div class="col">
        <div class="bg-light"><span id="present"></span></div>
    </div>
</div>
<div class="row gx-0 mb-2">
    <div class="col">
        <div class="bg-light">vmax Ingoing: </div>
    </div>
    <div class="col">
        <div class="bg-light"><span id="vmaxin"></span></div>
    </div>
</div>
<div class="row gx-0 mb-2">
    <div class="col">
        <div class="bg-light">vmax Outgoing: </div>
    </div>
    <div class="col">
        <div class="bg-light"><span id="vmaxout"></span></div>
    </div>
</div>

<div class="d-grid gap-2">
    <a class="btn btn-primary btn-block" href="/settings" role="button">Settings</a>
</div>

<script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', onLoad);
    function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
    }
    function onOpen(event) {
        console.log('Connection opened');
        //websocket.send('dataRequired');
    }
    function onClose(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }
    function onMessage(event) {
        var data = JSON.parse(event.data);
        document.getElementById("devicename").innerHTML = 'Device: ' + data.device_name;
        document.getElementById("amountin").innerHTML = data.amountIn;
        document.getElementById("amountout").innerHTML = data.amountOut;
        document.getElementById("present").innerHTML = data.present;
        document.getElementById("vmaxin").innerHTML = data.vmaxin + ' km/h';
        document.getElementById("vmaxout").innerHTML = data.vmaxout + ' km/h';
    }

    function onLoad(event) {
        initWebSocket();
    }

</script>
)rawliteral";