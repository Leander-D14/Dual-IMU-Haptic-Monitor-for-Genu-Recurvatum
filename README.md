# Dual-IMU Haptic Monitor for Genu Recurvatum

## Introduction

### Clinical Challenge: Genu Recurvatum
Genu recurvatum, or knee hyperextension, is a highly relevant clinical problem affecting an estimated 40 to 68% of post-stroke patients with hemiparesis [1]. This phenomenon primarily occurs during the stance phase of walking and is highly harmful, requiring targeted clinical intervention for several reasons:
* **Tissue Damage:** It causes excessive strain on the ligaments and tendons at the back of the knee, which can lead to significant pain and chronic tissue damage, as well as accelerated degradation of knee cartilage components [2].
* **Gait Inefficiency:** It results in an inefficient gait; because the knee does not bend normally, the leg effectively becomes longer. This raises the body's center of gravity, disrupts symmetry, and leads to a much higher metabolic energy expenditure during ambulation.
* **Proprioceptive Loss:** Many stroke patients develop this abnormality because a loss of internal proprioception means they simply no longer physically feel when their knee joint reaches or exceeds the anatomical limit of 0 degrees [1].

### The Haptic Advantage
For this specific pathological challenge, haptic technology is ideally suited to act as an **external haptic sense**. Because the patient lacks the internal sensory warning that the knee is hyperextending, an external vibrotactile stimulus can directly substitute for this missing sensory feedback loop [4]. 

Unlike visual screens, which are highly distracting and dangerous during active walking, or audible alarms, which can be socially disruptive and difficult to perceive in noisy environments, haptic feedback provides a physical, intuitive, and discrete warning that directly prompts the patient to correct their movement in real time [6], [7].

### Limitations of Current Solutions
In standard clinical practice, primarily passive mechanical or invasive medical solutions are utilized, each possessing distinct drawbacks:
* **Rigid Orthoses:** Splints and knee orthoses, such as the *"Swedish knee cage"* or full rigid leg braces, physically block hyperextension [3]. However, they are bulky, uncomfortable, and notoriously difficult to don and doff for stroke patients who often have only one fully functional upper limb.
* **Invasive Interventions:** Botulinum toxin injections or surgical tendon lengthenings are sometimes applied, but these are highly invasive and primarily target severe spasticity rather than correcting underlying motor learning [1].
* **Audible Biofeedback:** Prior literature has experimented with kinematic biofeedback via electrogoniometers [4]. While effective at retraining gait, these systems are often cumbersome, rely on restrictive physical hinges placed directly across the joint line, and use disruptive acoustic beeps that lower user acceptance in social settings.

### Proposed Solution: The Dual-IMU Haptic Monitor
The **Dual-IMU Haptic Monitor** project offers an innovative, non-restrictive alternative to these traditional methods by leveraging smart, wearable sensor technology. By shifting from a mechanical constraint to an active biofeedback system, it introduces several key innovations:

1. **Hingeless Angle Tracking:** A physical hinge is no longer necessary. Instead of a rigid goniometer aligned with the joint, the system utilizes two independent MPU6050 IMU sensors mounted on the calf and thigh segments, calculating the relative knee angle electronically to maximize user comfort.
2. **Context-Aware Gait Phase Detection:** Genu recurvatum is exclusively dangerous when the lower limb is bearing the patient's body weight (the stance phase). The system features an algorithm that detects a **Heel Strike** via an acceleration threshold exceeding 1.5g. Consequently, haptic warnings are strictly context-aware, arming the vibration logic only during the stance phase when the joint is under active load.
3. **Advanced Sensor Fusion (4D Quaternions):** To eliminate spatial mounting errors caused by shifting Velcro straps and to completely avoid gyroscopic drift, the architecture bypasses simple 1D angle coupling. It implements a robust Madgwick AHRS algorithm operating in 4D quaternion space, isolating the true anatomical knee kinematics regardless of minor sensor misalignment.
4. **Hybrid Feedback Loop:** The disruptive audible alarm of previous setups is replaced by precise, proportional tactile vibration profiles delivered via high-fidelity *Drake Haptic Actuators* [6]. This data is simultaneously streamed via telemetry to a real-time **3D Digital Twin** in Unity. This creates a powerful dual-loop system: the patient receives physical (proprioceptive) guidance, while the physical therapist receives immediate visual (exteroceptive) confirmation of hyperextension and gait asymmetry for diagnostic mapping.

## Supplies
[cite_start][Geef een gedetailleerde bill of materials (BOM) zodat anderen het prototype kunnen reproduceren[cite: 86]. [cite_start]Vermeld componenten, partnummers, links en geschatte kosten[cite: 87].]

| Component | Quantity | Purpose |
| :--- | :--- | :--- |
| Arduino Micro | 1 | [cite_start]Real-time control logic and sensor fusion [cite: 24] |
| MPU6050 IMU | 2 | [cite_start]Gait phase and orientation detection [cite: 24] |
| Drake Haptic Actuator | 1 | [cite_start]Vibrotactile cues [cite: 24] |
| DRV2605L Haptic Driver | 1 | Actuator driver |
| Jumper wires & Breadboard | 1 | [cite_start]System integration [cite: 24] |
| Velcro straps | 2 | [cite_start]One for calf, one for thigh: component placement [cite: 24] |

## Methods (Step 1 to n)
[cite_start][Beschrijf het conceptuele raamwerk achter je ontwerp[cite: 88]. [cite_start]Leg uit hoe elk onderdeel is geïntegreerd en waarom je bepaalde componenten hebt gekozen[cite: 89].]
[cite_start][Voeg hier afbeeldingen, diagrammen of flowcharts toe[cite: 90].]
[cite_start][Focussen op de praktische stappen van constructie en programmeren (bijv. het Complementary Filter)[cite: 92, 93].]
[cite_start][Bespreek hoe je hardware-beperkingen (zoals de I2C adressering op de bus) hebt opgelost[cite: 94].]

## Discussion (Step n+1)
[cite_start][Interpreteer de observaties van je prototype[cite: 97]. [cite_start]Beschrijf hoe goed jullie oplossing het medische probleem aanpakt en noteer onverwachte beperkingen[cite: 98].]

## Conclusion and future work (Step n+2)
[cite_start][Vat de belangrijkste bevindingen samen[cite: 100]. [cite_start]Stel verbeteringen voor en benadruk de lessen die nuttig zijn voor anderen die op dit werk willen voortbouwen[cite: 101].]


## References

[1] C. Bleyenheuft, Y. Bleyenheuft, P. Hanson, and T. Deltombe, "Treatment of genu recurvatum in hemiparetic adult patients: A systematic literature review," *Annals of Physical and Rehabilitation Medicine*, vol. 53, no. 3, pp. 189–199, Apr. 2010, doi: 10.1016/j.rehab.2010.01.001.

[2] W. Li et al., "Change in knee cartilage components in stroke patients with genu recurvatum analysed by zero TE MR imaging," *Scientific Reports*, vol. 12, no. 1, p. 3751, Mar. 2022, doi: 10.1038/s41598-022-07817-w.

[3] Z. Chen, Z. Xian, H. Chen, Y. Zhong, and F. Wang, "Immediate effects of a buffered knee orthosis on gait in stroke patients with knee hyperextension," *Journal of Back and Musculoskeletal Rehabilitation*, vol. 36, no. 2, pp. 445–454, Mar. 2023, doi: 10.3233/BMR-220069.

[4] J. Spencer, S. L. Wolf, and T. M. Kesar, "Biofeedback for Post-stroke Gait Retraining: A Review of Current Evidence and Future Research Directions in the Context of Emerging Technologies," *Frontiers in Neurology*, vol. 12, p. 637199, Mar. 2021, doi: 10.3389/fneur.2021.637199.

[5] T. Kobayashi, M. S. Orendurff, M. L. Singer, F. Gao, W. K. Daly, and K. B. Foreman, "Reduction of genu recurvatum through adjustment of plantarflexion resistance of an articulated ankle-foot orthosis in individuals post stroke," *Clinical Biomechanics*, vol. 35, pp. 81–85, Jun. 2016, doi: 10.1016/j.clinbiomech.2016.04.011.

[6] A. A. De Angelis et al., "Vibrotactile-based rehabilitation on balance and gait in patients with neurological diseases: A systematic review and metanalysis," *Brain Sciences*, vol. 11, no. 4, p. 518, Apr. 2021, doi: 10.3390/brainsci11040518.

[7] D. K. Y. Chen, M. Haller, and T. F. Besier, "Wearable lower limb haptic feedback device for retraining foot progression angle and step width," *Gait & Posture*, vol. 55, pp. 177–183, Jun. 2017, doi: 10.1016/j.gaitpost.2017.04.028.

[8] InvenSense Inc., "MPU-6000 and MPU-6050 Product Specification Revision 3.4," SparkFun Electronics, Product Datasheet, Aug. 2013. [Online]. Available: https://cdn.sparkfun.com/datasheets/Components/General%20IC/PS-MPU-6000A.pdf

[9] Texas Instruments, "DRV2605L 2-V to 5.2-V Haptic Driver for LRA and ERM With Internal Waveform Memory and Smart Loop Architecture," TI Product Folder, Datasheet, 2014–2020. [Online]. Available: https://www.ti.com/product/DRV2605L

[10] M. Ludwig, K. Chmielewska, and M. Bogusz, "Evaluation of AHRS algorithms for inertial personal localization in industrial environments," in *Proceedings of the International Conference on Inertial Sensors and Systems*, Oct. 2015, doi: 10.13140/RG.2.1.3414.9841.
