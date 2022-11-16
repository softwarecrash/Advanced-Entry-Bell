const char HTML_SETTINGS_EDIT[] PROGMEM = R"rawliteral(
<figure class="text-center">
    <h1>Edit Configuration</h1>
</figure>
<form method="POST" action="/settingssave" enctype="multipart/form-data">
    <div class="input-group mb-3">
        <span class="input-group-text w-50" id="devicenamedesc">Device Name</span>
        <input type="text" class="form-control" aria-describedby="devicenamedesc" id="devicename" name="post_deviceName"
            value="">
    </div>
    <div class="input-group mb-2">
        <span class="input-group-text w-50" id="cooldowntimedesc">Cooldown Time (ms)</span>
        <input type="text" class="form-control" aria-describedby="cooldowntimedesc" id="cooldowntime" name="post_cooldownTime"
            value="">
    </div>
    <div class="input-group mb-2">
        <span class="input-group-text w-50" id="bellsignaltimedesc">Bell Signal Time (ms)</span>
        <input type="text" class="form-control" aria-describedby="bellsignaltimedesc" id="bellsignaltime" name="post_bellSignalTime"
            value="">
    </div>
    <div class="input-group mb-2">
        <span class="input-group-text w-50" id="signaltimeoutdesc">Sensor Signal Timeout (ms)</span>
        <input type="text" class="form-control" aria-describedby="signaltimeoutdesc" id="signaltimeout" name="post_signalTimeout"
            value="">
    </div>
    <div class="d-grid gap-2">
        <input class="btn btn-primary" type="submit" value="Save settings">
</form>
<a class="btn btn-primary" href="/settings" role="button">Back</a>
</div>
<script>
    $(document).ready(function (load) {
        $.ajax({
            url: "settingsjson",
            data: {},
            type: "get",
            dataType: "json",
            cache: false,
            success: function (data) {
                document.getElementById("devicename").value = data.devicename;
                document.getElementById("cooldowntime").value = data.cooldowntime;
                document.getElementById("bellsignaltime").value = data.bellsignaltime;
                document.getElementById("signaltimeout").value = data.signaltimeout;
            }
        });
    });
</script>
)rawliteral";