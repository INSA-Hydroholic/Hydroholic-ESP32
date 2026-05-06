#pragma once

const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hydroholic Setup</title>
    <style>
        /* Modern Dark Theme */
        :root {
            --bg-color: #121212;
            --card-bg: #1e1e1e;
            --text-color: #e0e0e0;
            --accent-color: #00bcd4; /* Hydro cyan */
            --border-color: #333;
        }

        body {
            font-family: system-ui, -apple-system, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            padding: 1rem;
            box-sizing: border-box;
        }

        .container {
            background-color: var(--card-bg);
            padding: 2rem;
            border-radius: 12px;
            box-shadow: 0 8px 24px rgba(0,0,0,0.5);
            width: 100%;
            max-width: 400px;
        }

        h1 {
            text-align: center;
            color: var(--accent-color);
            margin-top: 0;
            margin-bottom: 0.5rem;
            font-size: 1.8rem;
        }

        .subtitle {
            text-align: center;
            color: #888;
            font-size: 0.9rem;
            margin-bottom: 2rem;
        }

        /* Custom Radio Buttons for Mode Selection */
        .mode-selector {
            display: flex;
            gap: 1rem;
            margin-bottom: 2rem;
        }

        .mode-selector label {
            flex: 1;
            text-align: center;
            background: var(--bg-color);
            border: 2px solid var(--border-color);
            padding: 0.75rem 0;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s ease;
            font-weight: 600;
        }

        .mode-selector input[type="radio"] {
            display: none;
        }

        .mode-selector input[type="radio"]:checked + label {
            border-color: var(--accent-color);
            color: var(--accent-color);
            background: rgba(0, 188, 212, 0.1);
        }

        /* Form Sections */
        .form-section {
            display: none;
            animation: fadeIn 0.4s ease;
        }

        .form-section.active {
            display: block;
        }

        .form-group {
            margin-bottom: 1.25rem;
        }

        label {
            display: block;
            margin-bottom: 0.5rem;
            font-size: 0.9rem;
            color: #bbb;
        }

        input[type="text"],
        input[type="password"] {
            width: 100%;
            padding: 0.75rem;
            background-color: var(--bg-color);
            border: 1px solid var(--border-color);
            color: var(--text-color);
            border-radius: 6px;
            box-sizing: border-box;
            font-size: 1rem;
            transition: border-color 0.3s;
        }

        input:focus {
            outline: none;
            border-color: var(--accent-color);
        }

        .help-text {
            display: block;
            font-size: 0.8rem;
            color: #666;
            margin-top: 0.3rem;
        }

        .ble-status {
            text-align: center;
            padding: 2rem;
            border: 1px dashed var(--border-color);
            border-radius: 6px;
            color: #888;
            margin-bottom: 1rem;
        }

        button[type="submit"] {
            width: 100%;
            padding: 1rem;
            background-color: var(--accent-color);
            color: #000;
            border: none;
            border-radius: 6px;
            font-size: 1rem;
            font-weight: bold;
            cursor: pointer;
            margin-top: 1rem;
            transition: background-color 0.2s;
        }

        button[type="submit"]:hover {
            background-color: #0097a7;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(-5px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>

    <div class="container">
        <h1>Hydroholic</h1>
        <div class="subtitle">Hydrobase Configuration</div>

        <form action="/save" method="POST">
            
            <!-- Mode Selection -->
            <div class="mode-selector">
                <input type="radio" id="mode-wifi" name="mode" value="wifi" checked>
                <label for="mode-wifi">WiFi</label>

                <input type="radio" id="mode-ble" name="mode" value="ble">
                <label for="mode-ble">Bluetooth (BLE)</label>
            </div>

            <!-- WiFi Configuration Form -->
            <div id="section-wifi" class="form-section active">
                <div class="form-group">
                    <label for="ssid">WiFi Network (SSID)</label>
                    <input type="text" id="ssid" name="ssid" placeholder="Enter network name" required>
                </div>
                
                <div class="form-group">
                    <label for="password">WiFi Password</label>
                    <input type="password" id="password" name="password" placeholder="Enter password">
                </div>

                <div class="form-group">
                    <label for="api_url">API URL (Optional)</label>
                    <input type="text" id="api_url" name="api_url" value="agir.minettoar.org/api">
                    <span class="help-text">Leave as default unless using a custom backend.</span>
                </div>
            </div>

            <!-- BLE Configuration Form -->
            <div id="section-ble" class="form-section">
                <div class="ble-status">
                    <p>BLE Mode Selected</p>
                    <span class="help-text">The device will broadcast as "Hydrobase_BLE" awaiting client connection.</span>
                </div>
                <!-- Add any specific BLE settings here if needed in the future -->
            </div>

            <button type="submit">Save & Reboot</button>
        </form>
    </div>

    <script>
        // JavaScript to handle the dynamic unhiding of form elements
        const radioWifi = document.getElementById('mode-wifi');
        const radioBle = document.getElementById('mode-ble');
        const sectionWifi = document.getElementById('section-wifi');
        const sectionBle = document.getElementById('section-ble');
        const wifiInput = document.getElementById('ssid'); // Used to toggle the required attribute

        function toggleSections() {
            if (radioWifi.checked) {
                sectionWifi.classList.add('active');
                sectionBle.classList.remove('active');
                wifiInput.setAttribute('required', 'true');
            } else {
                sectionWifi.classList.remove('active');
                sectionBle.classList.add('active');
                wifiInput.removeAttribute('required');
            }
        }

        // Add event listeners to the radio buttons
        radioWifi.addEventListener('change', toggleSections);
        radioBle.addEventListener('change', toggleSections);
    </script>

</body>
</html>
)rawliteral";