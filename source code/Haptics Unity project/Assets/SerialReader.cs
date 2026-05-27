using System.Globalization;
using System.IO.Ports;
using System.Threading;
using UnityEngine;

public class SerialReader : MonoBehaviour
{
    [Header("Port Settings")]
    public string portName = "COM9"; 
    public int baudRate = 115200;

    [Header("Rig References")]
    public Transform kneePivot;         
    public MeshRenderer[] legRenderers; 

    [Header("Visual Feedback (Gradient)")]
    public Gradient hyperextensionGradient;

    [Header("Hardware Settings (Press 'S' to Sync)")]
    public float warningAngle = 5f;
    public float criticalAngle = 0f;
    public bool enableGaitPhase = false;
    public float impactThreshold = 1.5f;

    [Header("Real-time 1D Data (Pitch Only)")]
    public float rawThighPitch;
    public float rawKneeAngle;

    [Header("Calibrated Output")]
    public float calibratedThigh;
    public float calibratedKnee;
    
    // Calibration offsets for the 1D plane
    private float thighOffset = 0f;
    // private float kneeOffset = 0f;

    private SerialPort serialPort;
    private Thread serialThread;
    private bool isRunning = false;
    private string latestPacket = "";

    void Start()
    {
        serialPort = new SerialPort(portName, baudRate);
        serialPort.ReadTimeout = 50;

        try
        {
            serialPort.Open();
            isRunning = true;
            serialThread = new Thread(ReadSerialLoop);
            serialThread.Start();
            Debug.Log("Serial port successfully opened on " + portName);
        }
        catch (System.Exception e)
        {
            Debug.LogError("Failed to open serial port: " + e.Message);
        }
    }

    void ReadSerialLoop()
    {
        while (isRunning && serialPort != null && serialPort.IsOpen)
        {
            try 
            { 
                latestPacket = serialPort.ReadLine(); 
            }
            catch (System.TimeoutException) { } 
        }
    }

    void Update()
    {
        // 1. Process incoming serial packets
        if (!string.IsNullOrEmpty(latestPacket))
        {
            ParsePacket(latestPacket);
            latestPacket = ""; 
        }

        // 2. Trigger baseline zero-calibration via user input
        if (Input.GetKeyDown(KeyCode.Space))
        {
            CalibrateZeros();
        }

        // 3. Sync settings to Arduino
        if (Input.GetKeyDown(KeyCode.S))
        {
            SyncSettingsToArduino();
        }

        // 4. Compute calibrated 1D kinematics
        calibratedThigh = rawThighPitch - thighOffset;
        calibratedKnee = rawKneeAngle;

        // 5. Apply strictly 1D rotations (X-axis only, ignoring Y and Z drift completely)
        transform.localRotation = Quaternion.Euler(calibratedThigh, 0, 0);
        
        if (kneePivot != null)
        {
            // Knee is locally rotated on its hinge
            kneePivot.localRotation = Quaternion.Euler(calibratedKnee, 0, 0);
        }

        // 6. Visual feedback
        UpdateGradientColor();
    }

    void ParsePacket(string packet)
    {
        string[] blocks = packet.Split('|');
        
        foreach (string block in blocks)
        {
            string[] prefixAndData = block.Split(':');
            if (prefixAndData.Length != 2) continue;

            string prefix = prefixAndData[0];
            string[] values = prefixAndData[1].Split(',');

            // We explicitly ONLY grab the Pitch (values[0]) and ignore Roll and Yaw completely
            if (prefix == "T" && values.Length == 3) 
            {
                rawThighPitch = ParseFloat(values[0]);
            }
            else if (prefix == "A" && values.Length == 1) 
            {
                // The Arduino has already calculated the correct relative 1D angle safely
                rawKneeAngle = ParseFloat(values[0]);
            }
        }
    }

    float ParseFloat(string s)
    {
        float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out float result);
        return result;
    }

    void CalibrateZeros()
    {
        // Store the linear offset
        thighOffset = rawThighPitch;
        
        if (serialPort != null && serialPort.IsOpen)
        {
            serialPort.Write("C\n");
        }
        Debug.Log("System calibrated: 1D plane established as zero-degree baseline.");
    }

    void SyncSettingsToArduino()
    {
        if (serialPort != null && serialPort.IsOpen)
        {
            int gaitInt = enableGaitPhase ? 1 : 0;
            string command = $"S:{warningAngle.ToString(CultureInfo.InvariantCulture)}," +
                             $"{criticalAngle.ToString(CultureInfo.InvariantCulture)}," +
                             $"{gaitInt}," +
                             $"{impactThreshold.ToString(CultureInfo.InvariantCulture)}\n";
            
            serialPort.Write(command);
            Debug.Log("Hardware Settings Synced: " + command);
        }
    }

    void UpdateGradientColor()
    {
        float severity = Mathf.InverseLerp(warningAngle, criticalAngle, rawKneeAngle);
        Color targetColor = hyperextensionGradient.Evaluate(severity);

        foreach (MeshRenderer rend in legRenderers)
        {
            if (rend != null)
            {
                rend.material.color = targetColor;
            }
        }
    }

    void OnApplicationQuit()
    {
        isRunning = false;
        if (serialThread != null && serialThread.IsAlive) serialThread.Join();
        if (serialPort != null && serialPort.IsOpen) serialPort.Close();
    }
}