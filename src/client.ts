const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

async function connectToGourde() {
  try {
    const displayElement = document.getElementById('poids-valeur');
    console.log("Recherche de la gourde...");

    // 1. Scanner (Attention : le nom doit correspondre exactement à BLEDevice::init("...") dans ton C++)
    const device = await (navigator as any).bluetooth.requestDevice({
      filters: [{ name: "Hydroholic" }], // Change en "Hydroholic" si tu as changé le code ESP32
      optionalServices: [SERVICE_UUID]
    });

    const server = await device.gatt?.connect();
    const service = await server?.getPrimaryService(SERVICE_UUID);
    const characteristic = await service?.getCharacteristic(CHARACTERISTIC_UUID);

    // 2. Activer les notifications (Flux en temps réel)
    await characteristic?.startNotifications();
    console.log("Notifications activées !");

    // 3. Écouter les changements de valeur
    characteristic?.addEventListener('characteristicvaluechanged', (event: any) => {
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

  } catch (error) {
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