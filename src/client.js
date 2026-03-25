"use strict";
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
async function connectToGourde() {
    var _a;
    try {
        const displayElement = document.getElementById('poids-valeur');
        console.log("Recherche de la gourde...");
        // 1. Scanner (Attention : le nom doit correspondre exactement à BLEDevice::init("...") dans ton C++)
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ name: "Hydroholic" }],
            optionalServices: [SERVICE_UUID]
        });
        const server = await ((_a = device.gatt) === null || _a === void 0 ? void 0 : _a.connect());
        const service = await (server === null || server === void 0 ? void 0 : server.getPrimaryService(SERVICE_UUID));
        const characteristic = await (service === null || service === void 0 ? void 0 : service.getCharacteristic(CHARACTERISTIC_UUID));
        // 2. Activer les notifications (Flux en temps réel)
        await (characteristic === null || characteristic === void 0 ? void 0 : characteristic.startNotifications());
        console.log("Notifications activées !");
        // 3. Écouter les changements de valeur
        characteristic === null || characteristic === void 0 ? void 0 : characteristic.addEventListener('characteristicvaluechanged', (event) => {
            const value = event.target.value;
            const decoder = new TextDecoder('utf-8');
            const poids = decoder.decode(value);
            console.log("Nouveau poids reçu :", poids);
            // Mise à jour de l'interface HTML
            if (displayElement) {
                displayElement.innerText = poids;
            }
        });
        alert("Connecté à " + device.name);
    }
    catch (error) {
        console.error("Erreur Bluetooth : ", error);
        alert("Erreur : " + error);
    }
}
window.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('connect');
    if (btn) {
        btn.addEventListener('click', connectToGourde);
        console.log("Bouton prêt à l'emploi !");
    }
});
