using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Windows.Forms;
using ScottPlot;
using ScottPlot.WinForms;

// Resolve ambiguity between ScottPlot and WinForms types
using Timer = System.Windows.Forms.Timer;
using Label = System.Windows.Forms.Label;
using Color = System.Drawing.Color;
using FontStyle = System.Drawing.FontStyle;

namespace CANBusMonitor
{
    public class FormPID : Form
    {
        private SerialPort serialPort;

        // === Chart ===
        private FormsPlot plotControl;
        private List<double> timeData = new();
        private List<double> actualData = new();
        private List<double> setpointData = new();
        private List<double> errorData = new();
        private DateTime startTime;
        private Timer refreshTimer;

        // === PID Inputs ===
        private NumericUpDown nudKp, nudKi, nudKd;
        private NumericUpDown nudTarget, nudCPR;
        private Button btnSendPID, btnApplyTarget;
        private Button btnStartPID, btnStopPID;
        private Button btnStepTest, btnExportCSV, btnClearPlot;
        private Button btnBack;

        // === Stats Labels ===
        private Label lblStatRPM, lblStatError, lblStatOutput, lblStatIntegral;

        // === Step Response Labels ===
        private Label lblOvershoot, lblRiseTime, lblSettling, lblSSError;

        // === Step Response Analysis ===
        private bool stepTestActive = false;
        private double stepTarget = 0;
        private double stepStartTime = 0;
        private double stepPeakRPM = 0;
        private double stepRiseTime = -1;
        private double stepSettlingTime = -1;
        private bool stepRiseReached = false;

        // === Data Buffer (for CSV + analysis) ===
        private struct DataPoint
        {
            public double Time;
            public double Actual;
            public double Setpoint;
            public double Error;
            public double Output;
        }
        private List<DataPoint> allData = new();

        // === External data injection (called from Form1) ===
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public int LatestActualRPM { get; set; }
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public int LatestTargetRPM { get; set; }
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public int LatestError { get; set; }
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public int LatestOutput { get; set; }
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public bool HasNewData { get; set; }

        public FormPID(SerialPort port)
        {
            serialPort = port;
            InitUI();
            startTime = DateTime.Now;

            refreshTimer = new Timer();
            refreshTimer.Interval = 50; // 20 FPS
            refreshTimer.Tick += RefreshTimer_Tick;
            refreshTimer.Start();
        }

        private void InitUI()
        {
            this.Text = "⚙ PID Motor Tuning — Advanced";
            this.ClientSize = new Size(1100, 700);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(20, 20, 30);
            this.ForeColor = Color.White;
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;

            // ============================================================
            //  TOP BAR
            // ============================================================
            btnBack = MakeButton("← Back to Main", new Point(10, 8), new Size(140, 32), Color.FromArgb(60, 60, 80));
            btnBack.Click += (s, e) => this.Close();
            this.Controls.Add(btnBack);

            Label lblTitle = new Label()
            {
                Text = "PID MOTOR TUNING",
                Location = new Point(200, 10),
                AutoSize = true,
                Font = new Font("Consolas", 16, FontStyle.Bold),
                ForeColor = Color.FromArgb(255, 200, 50)
            };
            this.Controls.Add(lblTitle);

            // ============================================================
            //  CHART (Left, large area)
            // ============================================================
            GroupBox gbChart = new GroupBox()
            {
                Text = " Real-time Speed Plot ",
                Location = new Point(10, 48),
                Size = new Size(730, 440),
                ForeColor = Color.FromArgb(100, 200, 255),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            plotControl = new FormsPlot()
            {
                Location = new Point(5, 18),
                Size = new Size(720, 415)
            };

            // Configure plot style
            plotControl.Plot.FigureBackground.Color = ScottPlot.Color.FromHex("#14141E");
            plotControl.Plot.DataBackground.Color = ScottPlot.Color.FromHex("#1A1A2E");
            plotControl.Plot.Axes.Bottom.Label.Text = "Time (s)";
            plotControl.Plot.Axes.Left.Label.Text = "RPM";
            plotControl.Plot.Axes.Bottom.Label.ForeColor = ScottPlot.Color.FromHex("#AAAAAA");
            plotControl.Plot.Axes.Left.Label.ForeColor = ScottPlot.Color.FromHex("#AAAAAA");
            plotControl.Plot.Axes.Bottom.TickLabelStyle.ForeColor = ScottPlot.Color.FromHex("#888888");
            plotControl.Plot.Axes.Left.TickLabelStyle.ForeColor = ScottPlot.Color.FromHex("#888888");

            // Custom Legend outside the plot
            Label lblLegActual = new Label() { Text = "■ Actual RPM", ForeColor = Color.FromArgb(0, 255, 128), Location = new Point(480, 15), AutoSize = true, Font = new Font("Arial", 9, FontStyle.Bold), BackColor = Color.Transparent };
            Label lblLegTarget = new Label() { Text = "■ Setpoint RPM", ForeColor = Color.FromArgb(255, 68, 68), Location = new Point(590, 15), AutoSize = true, Font = new Font("Arial", 9, FontStyle.Bold), BackColor = Color.Transparent };

            gbChart.Controls.AddRange(new Control[] { plotControl, lblLegActual, lblLegTarget });
            this.Controls.Add(gbChart);

            // ============================================================
            //  PID PARAMETERS (Right panel)
            // ============================================================
            GroupBox gbPID = new GroupBox()
            {
                Text = " PID Parameters ",
                Location = new Point(750, 48),
                Size = new Size(340, 220),
                ForeColor = Color.FromArgb(100, 255, 150),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            int y = 28;
            AddParamRow(gbPID, "Kp:", ref nudKp, y, 0.00m, 999.99m, 0.05m, 0.01m); y += 36;
            AddParamRow(gbPID, "Ki:", ref nudKi, y, 0.00m, 999.99m, 0.01m, 0.01m); y += 36;
            AddParamRow(gbPID, "Kd:", ref nudKd, y, 0.00m, 999.99m, 0.00m, 0.01m); y += 40;

            btnSendPID = MakeButton("📤 Send PID", new Point(15, y), new Size(305, 35), Color.FromArgb(0, 120, 180));
            btnSendPID.Font = new Font("Arial", 10, FontStyle.Bold);
            btnSendPID.Click += BtnSendPID_Click;
            gbPID.Controls.Add(btnSendPID);

            this.Controls.Add(gbPID);

            // ============================================================
            //  TARGET & CONTROL (Right panel, below PID)
            // ============================================================
            GroupBox gbTarget = new GroupBox()
            {
                Text = " Target & Control ",
                Location = new Point(750, 275),
                Size = new Size(340, 213),
                ForeColor = Color.FromArgb(255, 180, 80),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            Label lblTargetTitle = new Label() { Text = "Target RPM:", Location = new Point(15, 30), AutoSize = true, Font = new Font("Consolas", 10), ForeColor = Color.LightGray };
            nudTarget = new NumericUpDown()
            {
                Location = new Point(135, 27), Width = 100,
                Minimum = -5000, Maximum = 5000, Value = 500,
                Font = new Font("Consolas", 11, FontStyle.Bold),
                BackColor = Color.FromArgb(30, 30, 45), ForeColor = Color.White
            };

            btnApplyTarget = MakeButton("Apply", new Point(245, 25), new Size(80, 30), Color.FromArgb(0, 130, 80));
            btnApplyTarget.Click += BtnApplyTarget_Click;

            Label lblCprTitle = new Label() { Text = "CPR:", Location = new Point(15, 68), AutoSize = true, Font = new Font("Consolas", 10), ForeColor = Color.LightGray };
            nudCPR = new NumericUpDown()
            {
                Location = new Point(135, 65), Width = 100,
                Minimum = 1, Maximum = 65535, Value = 1500,
                Font = new Font("Consolas", 11, FontStyle.Bold),
                BackColor = Color.FromArgb(30, 30, 45), ForeColor = Color.White
            };

            btnStartPID = MakeButton("▶ Start PID", new Point(15, 108), new Size(150, 40), Color.FromArgb(0, 140, 50));
            btnStartPID.Font = new Font("Arial", 11, FontStyle.Bold);
            btnStartPID.Click += BtnStartPID_Click;

            btnStopPID = MakeButton("■ Stop PID", new Point(175, 108), new Size(150, 40), Color.FromArgb(180, 40, 40));
            btnStopPID.Font = new Font("Arial", 11, FontStyle.Bold);
            btnStopPID.Click += BtnStopPID_Click;

            btnStepTest = MakeButton("⚡ Step Test", new Point(15, 158), new Size(150, 40), Color.FromArgb(150, 100, 0));
            btnStepTest.Font = new Font("Arial", 10, FontStyle.Bold);
            btnStepTest.Click += BtnStepTest_Click;

            Label lblCprHint = new Label() { Text = "(count/rev)", Location = new Point(245, 68), AutoSize = true, Font = new Font("Arial", 8), ForeColor = Color.Gray };

            gbTarget.Controls.AddRange(new Control[] { lblTargetTitle, nudTarget, btnApplyTarget, lblCprTitle, nudCPR, lblCprHint, btnStartPID, btnStopPID, btnStepTest });
            this.Controls.Add(gbTarget);

            // ============================================================
            //  REAL-TIME STATS (Bottom left)
            // ============================================================
            GroupBox gbStats = new GroupBox()
            {
                Text = " Real-time Stats ",
                Location = new Point(10, 496),
                Size = new Size(440, 90),
                ForeColor = Color.FromArgb(150, 200, 255),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            int sx = 15, sy = 25;
            lblStatRPM = MakeStatLabel(gbStats, "RPM:", sx, sy, Color.FromArgb(0, 255, 128)); sx += 140;
            lblStatError = MakeStatLabel(gbStats, "ERR:", sx, sy, Color.FromArgb(255, 100, 100)); sx += 140;
            lblStatOutput = MakeStatLabel(gbStats, "OUT:", sx, sy, Color.FromArgb(255, 200, 50));
            sx = 15; sy = 52;
            lblStatIntegral = MakeStatLabel(gbStats, "∫ERR:", sx, sy, Color.Gray);

            this.Controls.Add(gbStats);

            // ============================================================
            //  STEP RESPONSE ANALYSIS (Bottom center)
            // ============================================================
            GroupBox gbStep = new GroupBox()
            {
                Text = " Step Response Analysis ",
                Location = new Point(460, 496),
                Size = new Size(360, 90),
                ForeColor = Color.FromArgb(255, 150, 100),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            int rx = 15, ry = 22;
            lblOvershoot = MakeStatLabel(gbStep, "Overshoot:", rx, ry, Color.FromArgb(255, 120, 80));
            lblRiseTime = MakeStatLabel(gbStep, "Rise Time:", rx + 180, ry, Color.FromArgb(100, 200, 255));
            ry = 50;
            lblSettling = MakeStatLabel(gbStep, "Settling:", rx, ry, Color.FromArgb(100, 255, 100));
            lblSSError = MakeStatLabel(gbStep, "SS Error:", rx + 180, ry, Color.FromArgb(200, 200, 200));

            this.Controls.Add(gbStep);

            // ============================================================
            //  TOOL BUTTONS (Bottom right)
            // ============================================================
            GroupBox gbTools = new GroupBox()
            {
                Text = " Tools ",
                Location = new Point(830, 496),
                Size = new Size(260, 90),
                ForeColor = Color.FromArgb(180, 180, 200),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            btnExportCSV = MakeButton("💾 Export CSV", new Point(10, 22), new Size(115, 55), Color.FromArgb(50, 80, 120));
            btnExportCSV.Font = new Font("Arial", 9, FontStyle.Bold);
            btnExportCSV.Click += BtnExportCSV_Click;

            btnClearPlot = MakeButton("🗑 Clear Plot", new Point(135, 22), new Size(115, 55), Color.FromArgb(80, 50, 50));
            btnClearPlot.Font = new Font("Arial", 9, FontStyle.Bold);
            btnClearPlot.Click += BtnClearPlot_Click;

            gbTools.Controls.AddRange(new Control[] { btnExportCSV, btnClearPlot });
            this.Controls.Add(gbTools);

            // ============================================================
            //  STATUS BAR
            // ============================================================
            Label lblStatus = new Label()
            {
                Text = "⚙ PID Tuning Tool | Encoder → CAN → UART → Plot",
                Location = new Point(10, 594),
                Size = new Size(1080, 20),
                Font = new Font("Consolas", 8),
                ForeColor = Color.FromArgb(60, 60, 80),
                TextAlign = ContentAlignment.MiddleCenter
            };
            this.Controls.Add(lblStatus);
        }

        // ======================== REFRESH TIMER (20 FPS) ========================
        private void RefreshTimer_Tick(object? sender, EventArgs e)
        {
            if (!HasNewData) return;
            HasNewData = false;

            double t = (DateTime.Now - startTime).TotalSeconds;
            double actual = LatestActualRPM;
            double target = LatestTargetRPM;
            double error = LatestError;
            double output = LatestOutput;

            // Store data
            timeData.Add(t);
            actualData.Add(actual);
            setpointData.Add(target);
            errorData.Add(error);
            allData.Add(new DataPoint { Time = t, Actual = actual, Setpoint = target, Error = error, Output = output });

            // Update stats
            lblStatRPM.Text = $"{actual:F0}";
            lblStatError.Text = $"{error:F0}";
            lblStatOutput.Text = $"{output}%";

            // Running integral (approximate)
            if (allData.Count > 1)
            {
                double integralSum = 0;
                for (int i = 1; i < allData.Count; i++)
                {
                    double dt = allData[i].Time - allData[i - 1].Time;
                    integralSum += allData[i].Error * dt;
                }
                lblStatIntegral.Text = $"{integralSum:F1}";
            }

            // Step response analysis
            if (stepTestActive && stepTarget != 0)
            {
                double absTarget = Math.Abs(stepTarget);

                // Peak tracking
                double absActual = Math.Abs(actual);
                if (absActual > Math.Abs(stepPeakRPM))
                    stepPeakRPM = actual;

                // Overshoot
                double overshoot = (Math.Abs(stepPeakRPM) - absTarget) / absTarget * 100;
                if (overshoot > 0)
                    lblOvershoot.Text = $"{overshoot:F1}%";
                else
                    lblOvershoot.Text = "0%";

                // Rise time (10% → 90%)
                double elapsed = t - stepStartTime;
                if (!stepRiseReached && absActual >= absTarget * 0.9)
                {
                    stepRiseReached = true;
                    stepRiseTime = elapsed * 1000;
                    lblRiseTime.Text = $"{stepRiseTime:F0} ms";
                }

                // Settling time (error < 2% for 0.5s)
                double errPercent = Math.Abs(error) / absTarget * 100;
                if (stepSettlingTime < 0 && stepRiseReached && errPercent < 2.0)
                {
                    stepSettlingTime = elapsed * 1000;
                    lblSettling.Text = $"{stepSettlingTime:F0} ms";
                }
                else if (errPercent >= 2.0)
                {
                    stepSettlingTime = -1; // Reset if error goes back up
                }

                // Steady-state error (last 1s average)
                if (elapsed > 2.0)
                {
                    double ssSum = 0; int ssCount = 0;
                    for (int i = allData.Count - 1; i >= 0 && allData[i].Time > t - 1.0; i--)
                    {
                        ssSum += Math.Abs(allData[i].Error);
                        ssCount++;
                    }
                    if (ssCount > 0)
                        lblSSError.Text = $"{ssSum / ssCount:F1} RPM";
                }
            }

            // Update chart
            UpdateChart();
        }

        private void UpdateChart()
        {
            plotControl.Plot.Clear();

            if (timeData.Count < 2) return;

            double[] tArr = timeData.ToArray();
            double[] aArr = actualData.ToArray();
            double[] sArr = setpointData.ToArray();

            var sigActual = plotControl.Plot.Add.Scatter(tArr, aArr);
            sigActual.LineWidth = 2;
            sigActual.Color = ScottPlot.Color.FromHex("#00FF80");
            sigActual.MarkerSize = 0;
            sigActual.LegendText = "Actual RPM";

            var sigSetpoint = plotControl.Plot.Add.Scatter(tArr, sArr);
            sigSetpoint.LineWidth = 2;
            sigSetpoint.LinePattern = LinePattern.Dashed;
            sigSetpoint.Color = ScottPlot.Color.FromHex("#FF4444");
            sigSetpoint.MarkerSize = 0;

            // Hide the default legend since we use custom WinForms labels
            plotControl.Plot.HideLegend();

            // Auto-scroll: show last 15 seconds
            double latestT = tArr[^1];
            if (latestT > 15)
                plotControl.Plot.Axes.Bottom.Min = latestT - 15;
            else
                plotControl.Plot.Axes.Bottom.Min = 0;
            plotControl.Plot.Axes.Bottom.Max = latestT + 0.5;

            plotControl.Refresh();
        }

        // ======================== BUTTON HANDLERS ========================
        private void BtnSendPID_Click(object? sender, EventArgs e)
        {
            int kp_i = (int)(nudKp.Value * 100);
            int ki_i = (int)(nudKi.Value * 100);
            int kd_i = (int)(nudKd.Value * 100);
            string cmd = $"<PID_SET:{kp_i},{ki_i},{kd_i}>\n";
            SendCommand(cmd);
        }

        private void BtnApplyTarget_Click(object? sender, EventArgs e)
        {
            string cmd = $"<PID_TARGET:{(int)nudTarget.Value}>\n";
            SendCommand(cmd);
        }

        private void BtnStartPID_Click(object? sender, EventArgs e)
        {
            // Send CPR first, then start
            string cmdCPR = $"<PID_CPR:{(int)nudCPR.Value}>\n";
            SendCommand(cmdCPR);
            System.Threading.Thread.Sleep(20);
            SendCommand("<PID_START>\n");
        }

        private void BtnStopPID_Click(object? sender, EventArgs e)
        {
            SendCommand("<PID_STOP>\n");
            stepTestActive = false;
        }

        private void BtnStepTest_Click(object? sender, EventArgs e)
        {
            // Clear previous data and start fresh
            BtnClearPlot_Click(null, EventArgs.Empty);

            stepTestActive = true;
            stepTarget = (double)nudTarget.Value;
            stepStartTime = (DateTime.Now - startTime).TotalSeconds;
            stepPeakRPM = 0;
            stepRiseTime = -1;
            stepSettlingTime = -1;
            stepRiseReached = false;

            // Reset analysis labels
            lblOvershoot.Text = "---";
            lblRiseTime.Text = "---";
            lblSettling.Text = "---";
            lblSSError.Text = "---";

            // Send PID params + target + start
            BtnSendPID_Click(null, EventArgs.Empty);
            System.Threading.Thread.Sleep(20);
            BtnApplyTarget_Click(null, EventArgs.Empty);
            System.Threading.Thread.Sleep(20);
            BtnStartPID_Click(null, EventArgs.Empty);
        }

        private void BtnExportCSV_Click(object? sender, EventArgs e)
        {
            if (allData.Count == 0)
            {
                MessageBox.Show("No data to export.", "Export", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            SaveFileDialog sfd = new SaveFileDialog();
            sfd.Filter = "CSV Files|*.csv";
            sfd.FileName = $"PID_Data_{DateTime.Now:yyyyMMdd_HHmmss}.csv";
            if (sfd.ShowDialog() == DialogResult.OK)
            {
                using (StreamWriter sw = new StreamWriter(sfd.FileName))
                {
                    sw.WriteLine("Time_s,Setpoint_RPM,Actual_RPM,Error,Output_%");
                    foreach (var dp in allData)
                    {
                        sw.WriteLine($"{dp.Time:F3},{dp.Setpoint:F0},{dp.Actual:F0},{dp.Error:F0},{dp.Output:F0}");
                    }
                }
                MessageBox.Show($"Exported {allData.Count} points to:\n{sfd.FileName}", "Export", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void BtnClearPlot_Click(object? sender, EventArgs e)
        {
            timeData.Clear();
            actualData.Clear();
            setpointData.Clear();
            errorData.Clear();
            allData.Clear();
            startTime = DateTime.Now;
            stepTestActive = false;

            plotControl.Plot.Clear();
            plotControl.Refresh();

            lblStatRPM.Text = "0"; lblStatError.Text = "0";
            lblStatOutput.Text = "0%"; lblStatIntegral.Text = "0";
            lblOvershoot.Text = "---"; lblRiseTime.Text = "---";
            lblSettling.Text = "---"; lblSSError.Text = "---";
        }

        // ======================== HELPERS ========================
        private void SendCommand(string cmd)
        {
            if (serialPort != null && serialPort.IsOpen)
                serialPort.Write(cmd);
        }

        private void AddParamRow(GroupBox parent, string label, ref NumericUpDown nud, int y, decimal min, decimal max, decimal val, decimal step)
        {
            Label lbl = new Label()
            {
                Text = label, Location = new Point(15, y + 4), Width = 35,
                Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.LightGray
            };
            nud = new NumericUpDown()
            {
                Location = new Point(55, y), Width = 110,
                Minimum = min, Maximum = max, Value = val, Increment = step, DecimalPlaces = 2,
                Font = new Font("Consolas", 12, FontStyle.Bold),
                BackColor = Color.FromArgb(30, 30, 45), ForeColor = Color.White
            };

            parent.Controls.AddRange(new Control[] { lbl, nud });
        }

        private Label MakeStatLabel(GroupBox parent, string title, int x, int y, Color color)
        {
            Label lblTitle = new Label()
            {
                Text = title, Location = new Point(x, y + 2), AutoSize = true,
                Font = new Font("Consolas", 9), ForeColor = Color.Gray
            };
            Label lblValue = new Label()
            {
                Text = "0", Location = new Point(x + (int)(title.Length * 9) + 5, y),
                Width = 80, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = color
            };
            parent.Controls.AddRange(new Control[] { lblTitle, lblValue });
            return lblValue;
        }

        private Button MakeButton(string text, Point loc, Size size, Color bgColor)
        {
            Button btn = new Button()
            {
                Text = text, Location = loc, Size = size,
                FlatStyle = FlatStyle.Flat, BackColor = bgColor,
                ForeColor = Color.White, Font = new Font("Arial", 8.5f, FontStyle.Bold),
                Cursor = Cursors.Hand
            };
            btn.FlatAppearance.BorderSize = 0;
            btn.FlatAppearance.MouseOverBackColor = ControlPaint.Light(bgColor, 0.3f);
            btn.FlatAppearance.MouseDownBackColor = ControlPaint.Dark(bgColor, 0.2f);
            return btn;
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            refreshTimer.Stop();
            base.OnFormClosing(e);
        }
    }
}
