// UUIDs identiques à ton code ESP32 (attention à la casse)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

async function connectToGourde() {
  try {
    console.log("Recherche de la gourde");

    // 1. Scanner et demander l'accès à l'appareil
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ name: "MyESP32" }],
      optionalServices: [SERVICE_UUID]
    });

    // 2. Connexion au serveur GATT (l'ESP32)
    const server = await device.gatt?.connect();
    
    // 3. Récupérer le service
    const service = await server?.getPrimaryService(SERVICE_UUID);

    // 4. Récupérer la caractéristique (la variable "poids")
    const characteristic = await service?.getCharacteristic(CHARACTERISTIC_UUID);

    // 5. Lire la valeur actuelle
    const value = await characteristic?.readValue();
    const decoder = new TextDecoder('utf-8');
    
    console.log("Donnée reçue : ", decoder.decode(value));

  } catch (error) {
    console.error("Erreur Bluetooth : ", error);
  }
}