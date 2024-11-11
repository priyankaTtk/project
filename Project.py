import json
import firebase_admin
from firebase_admin import credentials, firestore
import paho.mqtt.client as mqtt
from datetime import datetime

# MQTT Broker and topics
MQTT_BROKER = "broker.emqx.io"
CONTROL_TOPIC = "ArduinoTrafficController"
COMMAND_TOPIC = "NanoTrafficCommand"

# Firebase Setup
cred = credentials.Certificate("/home/dell/Project/serviceAccountKey.json")  # Replace with the path to your service account key file
firebase_admin.initialize_app(cred)
db = firestore.client()

# Initialize MQTT client
client = mqtt.Client()

# Firestore function to log lane activity in respective collections
def log_lane_activity(lane):
    # Log the lane activity to the corresponding collection (Lane A or Lane B)
    lane_collection = "LaneA" if lane == "Lane A" else "LaneB"
    data = {
        "lane": lane,
        "timestamp": datetime.now()  # Automatically add a timestamp when lane becomes active
    }
    db.collection(lane_collection).add(data)
    print(f"Logged data to Firestore ({lane_collection}): {data}")

# MQTT callback functions
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker")
        client.subscribe(CONTROL_TOPIC)
    else:
        print(f"Connection failed with code {rc}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        print("Received message:", data)
        
        if data.get("command") == "manual":
            if "lane" in data:
                # Forward the manual control command to the Arduino
                command_data = { "mode": "manual", "lane": data["lane"] }
                client.publish(COMMAND_TOPIC, json.dumps(command_data))
                print(f"Forwarded manual control command for {data['lane']} to Arduino")
                log_lane_activity(data["lane"])  # Log lane activity to Firestore
        elif data.get("command") == "smart":
            # Forward the smart mode command to the Arduino
            command_data = { "mode": "smart" }
            client.publish(COMMAND_TOPIC, json.dumps(command_data))
            print("Forwarded smart mode command to Arduino")
            log_lane_activity("smart")  # Log mode switch to Firestore
    except json.JSONDecodeError:
        print("Failed to decode JSON from payload:", msg.payload)
    except Exception as e:
        print("An error occurred:", e)

# Set up the callback functions for MQTT client
client.on_connect = on_connect
client.on_message = on_message

# Connect to the MQTT broker and start the loop
client.connect(MQTT_BROKER, 1883)
client.loop_forever()
