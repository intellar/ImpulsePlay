#include "imu_handler.h"
#include <Wire.h>
#include <math.h>

// --- Objets et variables internes au module IMU ---
struct bmi160_dev bmi160dev;

// --- Quaternion Implementation ---
struct Quaternion { 
    float w, x, y, z; 

    void normalize() {
        float mag = sqrt(w*w + x*x + y*y + z*z);
        w /= mag; x /= mag; y /= mag; z /= mag;
    }
};

// Multiplie deux quaternions (interne au module)
static Quaternion quat_multiply(Quaternion q1, Quaternion q2) {
    Quaternion qr;
    qr.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
    qr.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
    qr.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
    qr.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
    return qr;
}

// Constantes pour la conversion des données brutes du gyroscope
// La sensibilité du gyroscope pour une plage de +/- 2000 dps est de 16.4 LSB/dps
const float GYRO_SENSITIVITY_2000_DPS = 16.4f;
const float DEGREES_TO_RADIANS = PI / 180.0f;

// Fait tourner un vecteur par un quaternion (interne au module)
static void quat_rotate_vector(const float v[3], const Quaternion& q, float out[3]) { // Garde les tableaux C pour la compatibilité avec l'existant
    // Pour faire tourner les vecteurs de base du monde afin qu'ils correspondent
    // à l'orientation du capteur, nous devons utiliser le conjugué du quaternion.
    // v_rotated = q* * v * q
    Quaternion q_conj = {q.w, -q.x, -q.y, -q.z};

    // Version optimisée de la multiplication v' = q_conjugué * v * q
    float uv_x = q_conj.y * v[2] - q_conj.z * v[1];
    float uv_y = q_conj.z * v[0] - q_conj.x * v[2];
    float uv_z = q_conj.x * v[1] - q_conj.y * v[0];

    out[0] = v[0] + 2.0f * (q_conj.w * uv_x + (q_conj.y * uv_z - q_conj.z * uv_y));
    out[1] = v[1] + 2.0f * (q_conj.w * uv_y + (q_conj.z * uv_x - q_conj.x * uv_z));
    out[2] = v[2] + 2.0f * (q_conj.w * uv_z + (q_conj.x * uv_y - q_conj.y * uv_x));
}

// Crée un quaternion qui représente la rotation entre deux vecteurs (interne au module)
static Quaternion quat_from_vectors(const float v1[3], const float v2[3]) { // Garde les tableaux C pour la compatibilité avec l'existant
    float a[3] = { v1[1]*v2[2] - v1[2]*v2[1], v1[2]*v2[0] - v1[0]*v2[2], v1[0]*v2[1] - v1[1]*v2[0] }; // v1 x v2
    float w = sqrt((v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]) * (v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2])) + (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]); // |v1|*|v2| + v1.v2
    Quaternion q = { w, a[0], a[1], a[2] };
    q.normalize();
    return q;
}

// Repère calibré (Quaternion identité au départ)
static Quaternion q_calib = {1, 0, 0, 0};

// --- Constantes pour le filtre complémentaire ---
const float COMPLEMENTARY_FILTER_ALPHA = 0.98f; // 98% Gyro, 2% Accel

// --- Variables pour la fusion de capteurs ---
static Quaternion q_orientation = {1, 0, 0, 0}; // Quaternion d'orientation global
static unsigned long last_update_time = 0;       // Pour le calcul du delta-temps (dt)

// Variables pour la calibration par moyennage (non utilisées dans la logique actuelle mais requises pour la compilation)
static long calib_sum_x = 0;
static long calib_sum_y = 0;
static long calib_sum_z = 0;
static int calib_sample_count = 0;
// --- Fonctions de rappel (callback) pour le driver BMI160 ---
static void user_delay_ms(uint32_t period) {
  delay(period);
}

static int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  Wire.beginTransmission(dev_id);
  Wire.write(reg_addr);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(dev_id, (uint8_t)len) != len) return -1;
  for (uint16_t i = 0; i < len; i++) data[i] = Wire.read();
  return 0;
}

static int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  Wire.beginTransmission(dev_id);
  Wire.write(reg_addr);
  for (uint16_t i = 0; i < len; i++) Wire.write(data[i]);
  Wire.endTransmission();
  return 0;
}

static void init_bmi160_sensor_driver_interface(void) {
    bmi160dev.id = BMI160_I2C_ADDR;
    bmi160dev.intf = BMI160_I2C_INTF;
    bmi160dev.read = user_i2c_read;
    bmi160dev.write = user_i2c_write;
    bmi160dev.delay_ms = user_delay_ms;
}

static int8_t configure_bmi160(void) {
    int8_t rslt;
    rslt = bmi160_init(&bmi160dev);
    if (rslt != BMI160_OK) return rslt;

    Serial.printf("Initialisation du BMI160 réussie ! Chip ID: 0x%X\n", bmi160dev.chip_id);
    if (bmi160dev.chip_id != 0xD1) {
        Serial.println("ERREUR: Mauvais Chip ID ! Ce n'est probablement pas un BMI160.");
        return BMI160_E_DEV_NOT_FOUND;
    }

    bmi160dev.accel_cfg.odr = BMI160_ACCEL_ODR_400HZ;
    bmi160dev.accel_cfg.range = BMI160_ACCEL_RANGE_2G;
    bmi160dev.accel_cfg.bw = BMI160_ACCEL_BW_NORMAL_AVG4;
    bmi160dev.accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;

    bmi160dev.gyro_cfg.odr = BMI160_GYRO_ODR_400HZ;
    bmi160dev.gyro_cfg.range = BMI160_GYRO_RANGE_2000_DPS;
    bmi160dev.gyro_cfg.bw = BMI160_GYRO_BW_NORMAL_MODE;
    bmi160dev.gyro_cfg.power = BMI160_GYRO_NORMAL_MODE;

    return bmi160_set_sens_conf(&bmi160dev);
}

bool init_imu() {
    Wire.begin(6, 7); // SDA = GPIO 6, SCL = GPIO 7
    Wire.setClock(400000);

    init_bmi160_sensor_driver_interface();
    delay(100);
    if (configure_bmi160() != BMI160_OK) {
        Serial.println("ERREUR: Impossible de communiquer avec le BMI160. Vérifiez le câblage !");
        return false;
    }
    Serial.println("BMI160 initialisé avec succès.");

    return true;
}

void process_imu_data(float final_x[3], float final_y[3], float final_z[3], float gravity_vec[3], float gyro_rad_s[3]) {
    // --- 1. Lecture des données brutes des deux capteurs ---
    struct bmi160_sensor_data accel, gyro;
    int8_t rslt = bmi160_get_sensor_data(BMI160_ACCEL_SEL | BMI160_GYRO_SEL, &accel, &gyro, &bmi160dev);
    if (rslt != BMI160_OK) {
        Serial.println("Erreur lecture Accel/Gyro!");
        return;
    }

    // --- 2. Appliquer la correspondance des axes aux données brutes ---
    // C'est ici que vous devez appliquer la correspondance que vous avez trouvée.
    // Exemple : si l'axe Y de l'IMU est inversé par rapport à l'écran :
    accel.y = -accel.y;
    gyro.y = -gyro.y;
    // CORRECTION : Si l'axe Z de l'IMU pointe vers le bas, l'inverser pour qu'il pointe vers le haut.
    accel.z = -accel.z;
    gyro.z = -gyro.z;
    // Adaptez ces lignes en fonction de vos observations.

    // --- 3. Calcul du delta-temps (dt) ---
    unsigned long now = micros();
    if (last_update_time == 0) {
        last_update_time = now;
        return; // Premier passage, on ne fait rien
    }
    float dt = (now - last_update_time) / 1000000.0f;
    last_update_time = now;

    // --- 4. Intégration du gyroscope ---
    // Conversion des données brutes du gyroscope en radians/seconde
    float gx = (gyro.x / GYRO_SENSITIVITY_2000_DPS) * DEGREES_TO_RADIANS;
    float gy = (gyro.y / GYRO_SENSITIVITY_2000_DPS) * DEGREES_TO_RADIANS;
    float gz = (gyro.z / GYRO_SENSITIVITY_2000_DPS) * DEGREES_TO_RADIANS;

    // Remplir le tableau de sortie avec les vitesses en rad/s
    gyro_rad_s[0] = gx;
    gyro_rad_s[1] = gy;
    gyro_rad_s[2] = gz;

    // Créer un quaternion de rotation à partir des vitesses angulaires
    Quaternion q_gyro;
    q_gyro.w = 1.0;
    q_gyro.x = gx * dt / 2.0f;
    q_gyro.y = gy * dt / 2.0f;
    q_gyro.z = gz * dt / 2.0f;

    // Appliquer la rotation du gyroscope à l'orientation actuelle
    q_orientation = quat_multiply(q_gyro, q_orientation);

    q_orientation.normalize(); // Normaliser pour éviter la dérive numérique

    // --- 5. Correction par l'accéléromètre (Filtre Complémentaire) ---
    // On calcule toujours le vecteur de gravité estimé pour le calcul de l'accélération linéaire.
    const float gravity_estimated_base[3] = {0, 0, 1};
    float gravity_estimated[3];
    quat_rotate_vector(gravity_estimated_base, q_orientation, gravity_estimated);

    float accel_mag_sq = (float)accel.x * accel.x + (float)accel.y * accel.y + (float)accel.z * accel.z;
    if (accel_mag_sq > (16384.f * 0.9f)*(16384.f * 0.9f) && accel_mag_sq < (16384.f * 1.1f)*(16384.f * 1.1f)) {
        float gravity_measured[3] = { (float)accel.x, (float)accel.y, (float)accel.z };
        float inv_mag = 1.0f / sqrt(accel_mag_sq);
        gravity_measured[0] *= inv_mag;
        gravity_measured[1] *= inv_mag;
        gravity_measured[2] *= inv_mag;

        Quaternion q_correction = quat_from_vectors(gravity_estimated, gravity_measured);

        float alpha = 1.0f - COMPLEMENTARY_FILTER_ALPHA;
        q_orientation.w = q_orientation.w * (1.0f - alpha) + q_correction.w * alpha;
        q_orientation.x = q_orientation.x * (1.0f - alpha) + q_correction.x * alpha;
        q_orientation.y = q_orientation.y * (1.0f - alpha) + q_correction.y * alpha;
        q_orientation.z = q_orientation.z * (1.0f - alpha) + q_correction.z * alpha;

        q_orientation.normalize(); // Re-normaliser après la correction
    }

    // --- 6. Calculer les axes finaux en faisant tourner les vecteurs de base ---
    const float base_x[3] = {1, 0, 0};
    const float base_y[3] = {0, 1, 0};
    const float base_z[3] = {0, 0, 1};
    quat_rotate_vector(base_x, q_orientation, final_x);
    quat_rotate_vector(base_y, q_orientation, final_y);
    quat_rotate_vector(base_z, q_orientation, final_z);

    // Remplir le vecteur gravité pour l'affichage (normalisé)
    gravity_vec[0] = accel.x / 16384.f; // Toujours utile pour le débogage
    gravity_vec[1] = accel.y / 16384.f;
    gravity_vec[2] = accel.z / 16384.f;
}

void start_axis_calibration() {
    // Cette fonction peut être utilisée pour réinitialiser la vue si nécessaire.
    q_orientation = {1, 0, 0, 0};
    Serial.println("Orientation réinitialisée.");
}