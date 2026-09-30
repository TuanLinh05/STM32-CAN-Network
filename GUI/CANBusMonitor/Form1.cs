using System;
using System.Drawing;
using System.IO.Ports;
using System.Windows.Forms;

namespace CANBusMonitor
{
    public partial class Form1 : Form
    {
        private SerialPort serialPort;
        private ComboBox cbPorts;
        private Button btnConnect, btnClear, btnPing;
        private RichTextBox rtbLog;
        private Button btnStartAdc, btnStopAdc;
        private Button btnFwd, btnRev, btnMotorStop, btnBrake;
        private TrackBar tbPwm;
        private Label lblPwmValue;

        private ProgressBar pbAdc1, pbAdc2, pbAdc3;
        private Label lblAdc1, lblAdc2, lblAdc3;

        // Motor telemetry UI
        private Label lblRpmValue, lblDirValue, lblDutyValue, lblEncValue;
        private ProgressBar pbMotorDuty;
        private Button btnPID;
        private FormPID? formPID = null;
        private string rxBuffer = "";

        public Form1()
        {
            InitializeComponent();
            InitCustomComponents();
        }

        private void InitCustomComponents()
        {
            this.Text = "CAN Bus Monitor & Control (USART)";
            this.ClientSize = new Size(980, 640);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(25, 25, 35);
            this.ForeColor = Color.White;
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;

            // ============================================================
            //  HÀNG 1: Connection (trên cùng, cao 50px)
            // ============================================================
            Panel pnlConnect = new Panel() { Location = new Point(10, 8), Size = new Size(960, 42), BackColor = Color.FromArgb(35, 35, 50) };
            pnlConnect.Paint += (s, e) => { e.Graphics.DrawRectangle(new Pen(Color.FromArgb(60, 60, 80)), 0, 0, pnlConnect.Width - 1, pnlConnect.Height - 1); };

            Label lblConn = new Label() { Text = "COM:", Location = new Point(10, 12), AutoSize = true, ForeColor = Color.LightGray };
            cbPorts = new ComboBox() { Location = new Point(50, 9), Width = 90, DropDownStyle = ComboBoxStyle.DropDownList };
            cbPorts.Items.AddRange(SerialPort.GetPortNames());

            btnConnect = MakeButton("Connect", new Point(150, 7), new Size(95, 28), Color.FromArgb(0, 100, 180));
            btnConnect.Click += BtnConnect_Click;

            Button btnRefresh = MakeButton("Refresh", new Point(255, 7), new Size(80, 28), Color.FromArgb(80, 80, 100));
            btnRefresh.Click += (s, e) => { cbPorts.Items.Clear(); cbPorts.Items.AddRange(SerialPort.GetPortNames()); };

            btnClear = MakeButton("Clear All", new Point(345, 7), new Size(85, 28), Color.FromArgb(80, 80, 100));
            btnClear.Click += (s, e) => {
                rtbLog.Clear();
                pbAdc1.Value = 0; pbAdc2.Value = 0; pbAdc3.Value = 0;
                lblAdc1.Text = "0.00 V"; lblAdc2.Text = "0.00 V"; lblAdc3.Text = "0.00 V";
                lblRpmValue.Text = "0"; lblDirValue.Text = "---"; lblDutyValue.Text = "0%";
                lblEncValue.Text = "0"; pbMotorDuty.Value = 0;
            };

            btnPing = MakeButton("Ping Board", new Point(440, 7), new Size(100, 28), Color.FromArgb(0, 130, 110));
            btnPing.Click += (s, e) => SendCommand("<CMD_PING>\n");

            pnlConnect.Controls.AddRange(new Control[] { lblConn, cbPorts, btnConnect, btnRefresh, btnClear, btnPing });

            // ============================================================
            //  HÀNG 2 TRÁI: CAN Logger (chiếm 50% chiều ngang)
            // ============================================================
            GroupBox gbLogger = new GroupBox()
            {
                Text = " CAN Sniffer Logger ",
                Location = new Point(10, 58),
                Size = new Size(470, 340),
                ForeColor = Color.FromArgb(100, 200, 100),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };
            rtbLog = new RichTextBox()
            {
                Location = new Point(8, 20),
                Size = new Size(454, 312),
                ReadOnly = true,
                Font = new Font("Consolas", 9.5f),
                BackColor = Color.FromArgb(15, 15, 20),
                ForeColor = Color.Lime,
                BorderStyle = BorderStyle.None
            };
            gbLogger.Controls.Add(rtbLog);

            // ============================================================
            //  HÀNG 2 PHẢI: ADC Voltage Monitor (3 thanh tiến độ)
            // ============================================================
            GroupBox gbAdc = new GroupBox()
            {
                Text = " ADC Voltage Monitor ",
                Location = new Point(490, 58),
                Size = new Size(480, 340),
                ForeColor = Color.FromArgb(100, 180, 255),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            int barY = 35;
            int barSpacing = 55;

            // PA1
            Label t1 = new Label() { Text = "PA1", Location = new Point(15, barY + 3), Width = 35, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.Cyan };
            pbAdc1 = new ProgressBar() { Location = new Point(55, barY), Size = new Size(300, 22), Maximum = 330, Style = ProgressBarStyle.Continuous };
            lblAdc1 = new Label() { Text = "0.00 V", Location = new Point(365, barY + 2), Width = 100, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.Cyan };

            // PA2
            Label t2 = new Label() { Text = "PA2", Location = new Point(15, barY + barSpacing + 3), Width = 35, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.Yellow };
            pbAdc2 = new ProgressBar() { Location = new Point(55, barY + barSpacing), Size = new Size(300, 22), Maximum = 330, Style = ProgressBarStyle.Continuous };
            lblAdc2 = new Label() { Text = "0.00 V", Location = new Point(365, barY + barSpacing + 2), Width = 100, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.Yellow };

            // PA3
            Label t3 = new Label() { Text = "PA3", Location = new Point(15, barY + barSpacing * 2 + 3), Width = 35, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.LimeGreen };
            pbAdc3 = new ProgressBar() { Location = new Point(55, barY + barSpacing * 2), Size = new Size(300, 22), Maximum = 330, Style = ProgressBarStyle.Continuous };
            lblAdc3 = new Label() { Text = "0.00 V", Location = new Point(365, barY + barSpacing * 2 + 2), Width = 100, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.LimeGreen };

            // Scale
            Label lblScale = new Label()
            {
                Text = "0V              1V              2V              3V  3.3V",
                Location = new Point(50, barY + barSpacing * 2 + 30),
                Width = 320, Font = new Font("Consolas", 7.5f), ForeColor = Color.Gray
            };

            gbAdc.Controls.AddRange(new Control[] { t1, pbAdc1, lblAdc1, t2, pbAdc2, lblAdc2, t3, pbAdc3, lblAdc3, lblScale });

            // ============================================================
            //  HÀNG 3: Control Panel (Slave 1 + Slave 2 cạnh nhau)
            // ============================================================
            // --- Slave 1: ADC ---
            GroupBox gbSlave1 = new GroupBox()
            {
                Text = " Slave 1 — ADC Reader (F407) ",
                Location = new Point(10, 406),
                Size = new Size(310, 90),
                ForeColor = Color.Cyan,
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            btnStartAdc = MakeButton("▶ Start ADC", new Point(15, 30), new Size(130, 40), Color.FromArgb(0, 120, 60));
            btnStartAdc.Font = new Font("Arial", 10, FontStyle.Bold);
            btnStartAdc.Enabled = false;
            btnStartAdc.Click += (s, e) => SendCommand("<CMD_START_ADC>\n");

            btnStopAdc = MakeButton("■ Stop ADC", new Point(160, 30), new Size(130, 40), Color.FromArgb(150, 50, 0));
            btnStopAdc.Font = new Font("Arial", 10, FontStyle.Bold);
            btnStopAdc.Enabled = false;
            btnStopAdc.Click += (s, e) => SendCommand("<CMD_STOP_ADC>\n");

            gbSlave1.Controls.AddRange(new Control[] { btnStartAdc, btnStopAdc });

            // --- Slave 2: Motor DC ---
            GroupBox gbSlave2 = new GroupBox()
            {
                Text = " Slave 2 — Motor DC (F429) ",
                Location = new Point(330, 406),
                Size = new Size(640, 90),
                ForeColor = Color.Orange,
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            Label lblSpeed = new Label() { Text = "Speed:", Location = new Point(15, 30), AutoSize = true, Font = new Font("Arial", 9), ForeColor = Color.LightGray };
            tbPwm = new TrackBar() { Location = new Point(65, 22), Width = 180, Minimum = 0, Maximum = 100, Value = 50, TickFrequency = 10, Enabled = false };
            tbPwm.Scroll += (s, e) => lblPwmValue.Text = $"{tbPwm.Value}%";
            lblPwmValue = new Label() { Text = "50%", Location = new Point(250, 28), Width = 45, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.White };

            btnFwd = MakeButton("▶ Forward", new Point(305, 22), new Size(100, 38), Color.FromArgb(0, 130, 50));
            btnFwd.Font = new Font("Arial", 9, FontStyle.Bold);
            btnFwd.Enabled = false;
            btnFwd.Click += (s, e) => SendCommand($"<PWM_FWD_{tbPwm.Value}>\n");

            btnRev = MakeButton("◀ Reverse", new Point(415, 22), new Size(100, 38), Color.FromArgb(180, 130, 0));
            btnRev.Font = new Font("Arial", 9, FontStyle.Bold);
            btnRev.Enabled = false;
            btnRev.Click += (s, e) => SendCommand($"<PWM_REV_{tbPwm.Value}>\n");

            btnMotorStop = MakeButton("■ Stop", new Point(525, 22), new Size(80, 38), Color.FromArgb(180, 30, 30));
            btnMotorStop.Font = new Font("Arial", 9, FontStyle.Bold);
            btnMotorStop.Enabled = false;
            btnMotorStop.Click += (s, e) => SendCommand("<PWM_STOP>\n");

            btnBrake = MakeButton("⚠ Brake", new Point(550, 22), new Size(80, 38), Color.FromArgb(100, 0, 0));
            btnBrake.Font = new Font("Arial", 9, FontStyle.Bold);
            btnBrake.Enabled = false;
            btnBrake.Click += (s, e) => SendCommand("<PWM_BRAKE>\n");

            // Sắp xếp lại đẹp: 4 nút cách đều
            int btnW = 80, gap = 5, startX = 300;
            btnFwd.Size = new Size(btnW + 5, 38); btnFwd.Location = new Point(startX, 28);
            btnRev.Size = new Size(btnW + 5, 38); btnRev.Location = new Point(startX + btnW + 5 + gap, 28);
            btnMotorStop.Size = new Size(btnW, 38); btnMotorStop.Location = new Point(startX + (btnW + 5 + gap) * 2, 28);
            btnBrake.Size = new Size(btnW, 38); btnBrake.Location = new Point(startX + (btnW + 5 + gap) * 2 + btnW + gap, 28);

            gbSlave2.Controls.AddRange(new Control[] { lblSpeed, tbPwm, lblPwmValue, btnFwd, btnRev, btnMotorStop, btnBrake });

            // ============================================================
            //  HÀNG 4: Motor Telemetry (Hiển thị vận tốc từ encoder)
            // ============================================================
            GroupBox gbMotorInfo = new GroupBox()
            {
                Text = " Motor Telemetry (Encoder F429) ",
                Location = new Point(10, 504),
                Size = new Size(960, 80),
                ForeColor = Color.FromArgb(255, 180, 50),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };

            // RPM
            Label lblRpmTitle = new Label() { Text = "RPM:", Location = new Point(15, 32), AutoSize = true, Font = new Font("Consolas", 10, FontStyle.Bold), ForeColor = Color.LightGray };
            lblRpmValue = new Label() { Text = "0", Location = new Point(65, 30), Width = 100, Font = new Font("Consolas", 16, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 128) };

            // Direction
            Label lblDirTitle = new Label() { Text = "DIR:", Location = new Point(180, 32), AutoSize = true, Font = new Font("Consolas", 10, FontStyle.Bold), ForeColor = Color.LightGray };
            lblDirValue = new Label() { Text = "---", Location = new Point(225, 30), Width = 100, Font = new Font("Consolas", 14, FontStyle.Bold), ForeColor = Color.White };

            // Duty
            Label lblDutyTitle = new Label() { Text = "DUTY:", Location = new Point(320, 32), AutoSize = true, Font = new Font("Consolas", 10, FontStyle.Bold), ForeColor = Color.LightGray };
            pbMotorDuty = new ProgressBar() { Location = new Point(380, 30), Size = new Size(180, 22), Maximum = 100, Style = ProgressBarStyle.Continuous };
            lblDutyValue = new Label() { Text = "0%", Location = new Point(570, 32), Width = 55, Font = new Font("Consolas", 11, FontStyle.Bold), ForeColor = Color.White };

            // Encoder count
            Label lblEncTitle = new Label() { Text = "ENC:", Location = new Point(640, 32), AutoSize = true, Font = new Font("Consolas", 10, FontStyle.Bold), ForeColor = Color.LightGray };
            lblEncValue = new Label() { Text = "0", Location = new Point(690, 30), Width = 100, Font = new Font("Consolas", 14, FontStyle.Bold), ForeColor = Color.FromArgb(100, 200, 255) };

            // Nút mở trang PID nâng cao (đặt ở góc phải của Motor Telemetry)
            btnPID = MakeButton("⚙ PID Tuning", new Point(810, 24), new Size(130, 40), Color.FromArgb(120, 50, 180));
            btnPID.Font = new Font("Arial", 10, FontStyle.Bold);
            btnPID.Enabled = false;
            btnPID.Click += (s, e) => {
                if (formPID == null || formPID.IsDisposed)
                {
                    formPID = new FormPID(serialPort);
                    formPID.Show();
                }
                else
                {
                    formPID.BringToFront();
                }
            };

            gbMotorInfo.Controls.AddRange(new Control[] { lblRpmTitle, lblRpmValue, lblDirTitle, lblDirValue, lblDutyTitle, pbMotorDuty, lblDutyValue, lblEncTitle, lblEncValue, btnPID });

            // ============================================================
            //  HÀNG 5: Thanh trạng thái
            // ============================================================
            Label lblStatus = new Label()
            {
                Text = "© CAN Bus Monitor v2.0 | STM32H743 + F407 + F429",
                Location = new Point(10, 590),
                Size = new Size(960, 20),
                Font = new Font("Consolas", 8),
                ForeColor = Color.FromArgb(80, 80, 100),
                TextAlign = ContentAlignment.MiddleCenter
            };

            // ============================================================
            //  ADD ALL TO FORM
            // ============================================================
            this.ClientSize = new Size(980, 620);
            this.Controls.AddRange(new Control[] { pnlConnect, gbLogger, gbAdc, gbSlave1, gbSlave2, gbMotorInfo, lblStatus });

            serialPort = new SerialPort();
            serialPort.BaudRate = 115200;
            serialPort.DataReceived += SerialPort_DataReceived;
        }

        // === Helper: Tạo nút đẹp ===
        private Button MakeButton(string text, Point loc, Size size, Color bgColor)
        {
            Button btn = new Button()
            {
                Text = text,
                Location = loc,
                Size = size,
                FlatStyle = FlatStyle.Flat,
                BackColor = bgColor,
                ForeColor = Color.White,
                Font = new Font("Arial", 8.5f, FontStyle.Bold),
                Cursor = Cursors.Hand
            };
            btn.FlatAppearance.BorderSize = 0;
            btn.FlatAppearance.MouseOverBackColor = ControlPaint.Light(bgColor, 0.3f);
            btn.FlatAppearance.MouseDownBackColor = ControlPaint.Dark(bgColor, 0.2f);
            return btn;
        }

        private void BtnConnect_Click(object sender, EventArgs e)
        {
            if (!serialPort.IsOpen)
            {
                if (cbPorts.SelectedItem == null)
                {
                    MessageBox.Show("Please select a COM port first.");
                    return;
                }
                try
                {
                    serialPort.PortName = cbPorts.SelectedItem.ToString();
                    serialPort.Open();
                    btnConnect.Text = "Disconnect";
                    btnConnect.BackColor = Color.FromArgb(180, 50, 50);
                    cbPorts.Enabled = false;
                    EnableControls(true);
                    Log("Connected to " + serialPort.PortName);

                    System.Threading.Thread.Sleep(100);
                    rxBuffer = "";
                    SendCommand("<CMD_PING>\n");
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error: " + ex.Message);
                }
            }
            else
            {
                serialPort.Close();
                btnConnect.Text = "Connect";
                btnConnect.BackColor = Color.FromArgb(0, 100, 180);
                cbPorts.Enabled = true;
                EnableControls(false);
                Log("Disconnected.");
            }
        }

        private void EnableControls(bool en)
        {
            btnStartAdc.Enabled = en;
            btnStopAdc.Enabled = en;
            tbPwm.Enabled = en;
            btnFwd.Enabled = en;
            btnRev.Enabled = en;
            btnMotorStop.Enabled = en;
            btnBrake.Enabled = en;
            btnPID.Enabled = en;
        }

        private void SendCommand(string cmd)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Write(cmd);
                Log("TX: " + cmd.TrimEnd('\n'));
            }
        }

        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            string incoming = serialPort.ReadExisting();
            this.Invoke((MethodInvoker)delegate
            {
                rxBuffer += incoming;

                while (rxBuffer.Contains("\n"))
                {
                    int idx = rxBuffer.IndexOf("\n");
                    string line = rxBuffer.Substring(0, idx).Trim('\r');
                    rxBuffer = rxBuffer.Substring(idx + 1);

                    if (string.IsNullOrWhiteSpace(line)) continue;

                    // Parse 3 kênh ADC
                    if (line.Contains("<ADC_3CH:") && line.Contains(">"))
                    {
                        int start = line.IndexOf("<ADC_3CH:") + 9;
                        int end = line.IndexOf(">", start);
                        if (end > start)
                        {
                            string[] parts = line.Substring(start, end - start).Split(',');
                            if (parts.Length == 3)
                            {
                                var ci = System.Globalization.CultureInfo.InvariantCulture;
                                if (float.TryParse(parts[0], System.Globalization.NumberStyles.Float, ci, out float v1) &&
                                    float.TryParse(parts[1], System.Globalization.NumberStyles.Float, ci, out float v2) &&
                                    float.TryParse(parts[2], System.Globalization.NumberStyles.Float, ci, out float v3))
                                {
                                    pbAdc1.Value = Math.Min(330, (int)(v1 * 100));
                                    pbAdc2.Value = Math.Min(330, (int)(v2 * 100));
                                    pbAdc3.Value = Math.Min(330, (int)(v3 * 100));
                                    lblAdc1.Text = $"{v1:0.00} V";
                                    lblAdc2.Text = $"{v2:0.00} V";
                                    lblAdc3.Text = $"{v3:0.00} V";
                                }
                            }
                        }
                    }
                    // Parse Motor telemetry: <MOTOR_SPD:rpm,dir,duty,enc>
                    else if (line.Contains("<MOTOR_SPD:") && line.Contains(">"))
                    {
                        int start = line.IndexOf("<MOTOR_SPD:") + 11;
                        int end = line.IndexOf(">", start);
                        if (end > start)
                        {
                            string[] parts = line.Substring(start, end - start).Split(',');
                            if (parts.Length == 4)
                            {
                                if (int.TryParse(parts[0], out int rpm) &&
                                    int.TryParse(parts[1], out int dir) &&
                                    int.TryParse(parts[2], out int duty) &&
                                    int.TryParse(parts[3], out int enc))
                                {
                                    lblRpmValue.Text = Math.Abs(rpm).ToString();
                                    lblRpmValue.ForeColor = rpm >= 0 ? Color.FromArgb(0, 255, 128) : Color.FromArgb(255, 100, 100);
                                    lblDirValue.Text = dir == 0 ? "FWD ▶" : "◀ REV";
                                    lblDirValue.ForeColor = dir == 0 ? Color.FromArgb(0, 200, 100) : Color.FromArgb(255, 180, 0);
                                    pbMotorDuty.Value = Math.Min(100, Math.Max(0, duty));
                                    lblDutyValue.Text = $"{duty}%";
                                    lblEncValue.Text = enc.ToString();
                                }
                            }
                        }
                    }
                    // Parse PID telemetry: <PID_DATA:target,actual,error,output>
                    else if (line.Contains("<PID_DATA:") && line.Contains(">"))
                    {
                        int start = line.IndexOf("<PID_DATA:") + 10;
                        int end = line.IndexOf(">", start);
                        if (end > start)
                        {
                            string[] parts = line.Substring(start, end - start).Split(',');
                            if (parts.Length == 4)
                            {
                                if (int.TryParse(parts[0], out int target) &&
                                    int.TryParse(parts[1], out int actual) &&
                                    int.TryParse(parts[2], out int error) &&
                                    int.TryParse(parts[3], out int output))
                                {
                                    // Forward to PID form
                                    if (formPID != null && !formPID.IsDisposed)
                                    {
                                        formPID.LatestTargetRPM = target;
                                        formPID.LatestActualRPM = actual;
                                        formPID.LatestError = error;
                                        formPID.LatestOutput = output;
                                        formPID.HasNewData = true;
                                    }
                                }
                            }
                        }
                    }
                    else if (line.Contains("[SYSTEM] STM32H743 MASTER CONNECTED!"))
                    {
                        Log("RX: " + line);
                        MessageBox.Show("Good connecting!\nH743 đã kết nối.\nCAN Bus 125kbps sẵn sàng.", "Status", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    else
                    {
                        Log("RX: " + line);
                    }
                }

                if (rxBuffer.Length > 500) { Log("RX [OVERFLOW]: " + rxBuffer); rxBuffer = ""; }
            });
        }

        private void Log(string msg)
        {
            rtbLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {msg}\n");
            rtbLog.SelectionStart = rtbLog.Text.Length;
            rtbLog.ScrollToCaret();
        }
    }
}
