using System.Globalization;
using System.IO.Ports;
using System.Threading;
using UnityEngine;

public class SerialReader : MonoBehaviour
{
    [Header("Port Settings")]
    public string portName = "COM3"; 
    public int baudRate = 115200;

    [Header("Rig References")]
    public Transform kneePivot;         
    public MeshRenderer[] legRenderers; 
    public Color normalColor = new Color(0.8f, 0.8f, 0.8f); 
    public Color warningColor = Color.red;                  

    [Header("Real-time Raw Data")]
    public float thighRoll;
    public float calfRoll;
    public float rawKneeAngle;

    [Header("Calibrated Output")]
    public float calibratedThigh;
    public float calibratedKnee;
    
    // Calibration offsets
    private float thighOffset = 0f;
    private float kneeOffset = 0f;

    private SerialPort serialPort;
    private Thread serialThread;
    private bool isRunning = false;
    private string latestPacket = "";

    void Start()
    {
        // Initialize the serial port with standard timeout parameters
        serialPort = new SerialPort(portName, baudRate);
        serialPort.ReadTimeout = 50;

        try
        {
            serialPort.Open();
            isRunning = true;
            
            // Execute serial read operations on a dedicated background thread 
            // to prevent frame rate degradation in the main render loop.
            serialThread = new Thread(ReadSerialLoop);
            serialThread.Start();
            Debug.Log("Serial port successfully opened on " + portName);
        }
        catch (System.Exception e)
        {
            Debug.LogError("Failed to open serial port: " + e.Message);
        }
    }

    // Continuous background data acquisition loop
    void ReadSerialLoop()
    {
        while (isRunning && serialPort != null && serialPort.IsOpen)
        {
            try 
            { 
                latestPacket = serialPort.ReadLine(); 
            }
            catch (System.TimeoutException) 
            { 
                // Timeout is expected when the hardware buffer is momentarily empty
            } 
        }
    }

    void Update()
    {
        // 1. Process incoming serial packets synchronously in the main thread
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

        // 3. Compute calibrated kinematics by applying baseline offsets
        calibratedThigh = thighRoll - thighOffset;
        calibratedKnee = rawKneeAngle - kneeOffset;

        // 4. Apply real-time rotations to the digital twin hierarchy
        // Local Z-axis rotation simulates the anatomical flexion/extension plane.
        transform.localRotation = Quaternion.Euler(calibratedThigh, 0, 0);

        if (kneePivot != null)
        {
            kneePivot.localRotation = Quaternion.Euler(calibratedKnee, 0, 0);
        }

        // 5. Visual feedback: Trigger warning state upon detecting hyperextension (angle < 0 deg)
        UpdateColors(calibratedKnee < 0f);
    }

    void ParsePacket(string packet)
    {
        // Expected incoming payload format: "D:val,K:val,A:val"
        string[] dataPairs = packet.Split(',');
        foreach (string pair in dataPairs)
        {
            string[] keyValue = pair.Split(':');
            if (keyValue.Length == 2)
            {
                // Enforce invariant culture parsing to ensure consistent floating-point deserialization
                if (float.TryParse(keyValue[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float value))
                {
                    if (keyValue[0] == "D") thighRoll = value;
                    else if (keyValue[0] == "K") calfRoll = value;
                    else if (keyValue[0] == "A") rawKneeAngle = value;
                }
            }
        }
    }

    void CalibrateZeros()
    {
        thighOffset = thighRoll;
        kneeOffset = rawKneeAngle;
        if (serialPort != null && serialPort.IsOpen)
        {
            serialPort.Write("C");
        }
        Debug.Log("System calibrated: Current physical orientation established as the zero-degree baseline.");
    }

    void UpdateColors(bool isHyperextended)
    {
        Color targetColor = isHyperextended ? warningColor : normalColor;
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
        // Ensure deterministic termination of background threads and hardware streams
        isRunning = false;
        if (serialThread != null && serialThread.IsAlive) 
        {
            serialThread.Join();
        }
        if (serialPort != null && serialPort.IsOpen) 
        {
            serialPort.Close();
        }
    }
}