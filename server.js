const express = require("express");
const bodyParser = require("body-parser");
const cors = require("cors");

const app = express();

app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

let relay1_state = false;
let relay2_state = false;

let temperature = 22.0;
let humidity = 50.0;

let commandLog = [];

app.get("/", (req, res) => {
  res.json({
    device: "ESP32 Emulator",
    status: "running",
    endpoints: ["/status", "/control", "/config", "/sensors", "/log"]
  });
});

app.get("/status", (req, res) => {
  res.json({
    relay1: relay1_state ? "on" : "off",
    relay2: relay2_state ? "on" : "off"
  });
});

app.post("/control", (req, res) => {
  const { device, state } = req.body;

  if (!device || !state) {
    return res.status(400).json({ success: false, message: "device and state are required" });
  }

  if (device === "relay1") relay1_state = (state === "on");
  if (device === "relay2") relay2_state = (state === "on");

  commandLog.push({
    timestamp: new Date().toISOString(),
    device,
    state
  });

  res.json({
    success: true,
    relay1: relay1_state ? "on" : "off",
    relay2: relay2_state ? "on" : "off"
  });
});
app.get("/config", (req, res) => {
  res.json({
    ip: "127.0.0.1",
    ssid: "LocalWiFi",
    status: "connected"
  });
});

app.get("/sensors", (req, res) => {
  temperature += (Math.random() - 0.5);
  humidity += (Math.random() - 0.5);

  if (temperature < 15) temperature = 15;
  if (temperature > 35) temperature = 35;
  if (humidity < 20) humidity = 20;
  if (humidity > 80) humidity = 80;

  res.json({
    temperature: parseFloat(temperature.toFixed(1)),
    humidity: parseFloat(humidity.toFixed(1))
  });
});

app.get("/log", (req, res) => {
  res.json(commandLog);
});

const PORT = 3000;
app.listen(PORT, () => {
  console.log(`ESP32 Emulator running at http://localhost:${PORT}`);
});
