from flask import Flask, request, jsonify
import psycopg2

app = Flask(__name__)

DB_CONFIG = {
    "host": "127.0.0.1",
    "dbname": "smart_car_runs",
    "user": "postgres",
    "password": "iloveflavi"
}

def get_db_connection():
    return psycopg2.connect(**DB_CONFIG)

@app.route("/sensor", methods=["POST"])
def receive_sensor_data():
    data = request.get_json()

    start_time = data.get("start_time")
    end_time = data.get("end_time")
    minutes = data.get("minutes")
    seconds = data.get("seconds")

    conn = get_db_connection()
    cur = conn.cursor()

    cur.execute(
        "INSERT INTO smart_car_table_updated (start_time, end_time, minutes, seconds) VALUES (%s, %s, %s, %s)",
        (start_time, end_time, minutes, seconds)
    )

    conn.commit()
    cur.close()
    conn.close()

    return jsonify({"status": "success"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
