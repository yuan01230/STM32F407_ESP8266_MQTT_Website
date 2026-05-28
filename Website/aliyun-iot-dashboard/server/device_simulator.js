const mqtt = require('mqtt');

// User's Aliyun IoT Device MQTT Credentials
const config = {
  host: 'iot-06z009yzvl7ads8.mqtt.iothub.aliyuncs.com',
  port: 1883,
  clientId: 'k0emt5tuoqi.Sensor|securemode=2,signmethod=hmacsha256,timestamp=1779110582660|',
  username: 'Sensor&k0emt5tuoqi',
  password: '0a9f072ec61c97c01a58bc03044be73a9bd9507721624a17bba5f04077cce890',
  productKey: 'k0emt5tuoqi',
  deviceName: 'Sensor'
};

const topic = `/sys/${config.productKey}/${config.deviceName}/thing/event/property/post`;

console.log('====================================================');
console.log(' 正在初始化阿里云物联网物理设备模拟器...');
console.log(` 连接目标: ${config.host}:${config.port}`);
console.log(` 设备身份: ${config.productKey} / ${config.deviceName}`);
console.log('====================================================');

// Connect to Aliyun MQTT Broker
const client = mqtt.connect(`mqtt://${config.host}:${config.port}`, {
  clientId: config.clientId,
  username: config.username,
  password: config.password,
  keepalive: 60,
  clean: true,
  reconnectPeriod: 5000
});

client.on('connect', () => {
  console.log('🟢 [SUCCESS] 物理模拟器已成功连入阿里云 MQTT Broker！设备已上线。');

  // Local state variables for simulation
  let temp = 25.0;
  let hum = 50.0;
  let yaw = 180.0;
  let roll = 0.0;
  let pitch = 0.0;
  let adc = 400;
  let led0 = 0;
  let led1 = 0;
  let vol = 2.5;

  const setTopic = `/sys/${config.productKey}/${config.deviceName}/thing/service/property/set`;
  const replyTopic = `/sys/${config.productKey}/${config.deviceName}/thing/service/property/set_reply`;

  // Subscribe to cloud downlinks
  client.subscribe(setTopic, (err) => {
    if (err) {
      console.error('🔴 [ERR] 订阅云端控制指令主题失败:', err.message);
    } else {
      console.log('📡 [SUB] 已成功订阅云端控制指令下发通道。');
    }
  });

  // Helper function to publish current properties
  function publishProperties(reason = '定时上报') {
    const payload = {
      id: String(Date.now()),
      version: '1.0',
      params: {
        temperature: temp,
        Humidity: hum,
        Yaw: yaw,
        Roll: roll,
        Pitch: pitch,
        Light_ADC: adc,
        Light_Out_Voltage: vol,
        LEDSwitch0: led0,
        LEDSwitch1: led1
      },
      method: 'thing.event.property.post'
    };

    client.publish(topic, JSON.stringify(payload), { qos: 0 }, (err) => {
      if (err) {
        console.error(`🔴 [ERR] 上报遥测数据失败 (${reason}):`, err.message);
      } else {
        console.log(`📤 [TX] (${reason}) 成功推送设备属性 -> 温度: ${temp}°C, LED0: ${led0}, LED1: ${led1}, 电压: ${vol}V`);
      }
    });
  }

  // Handle incoming commands
  client.on('message', (topicReceived, message) => {
    if (topicReceived === setTopic) {
      try {
        const data = JSON.parse(message.toString());
        console.log('📥 [RX] 收到云端下行物模型控制指令:', JSON.stringify(data.params));

        if (data.params) {
          let stateChanged = false;

          if (data.params.LEDSwitch0 !== undefined) {
            led0 = parseInt(data.params.LEDSwitch0) || 0;
            console.log(`💡 [ACTUATOR] LED0 主开关已设置为: ${led0 === 1 ? '开启 (ON)' : '关闭 (OFF)'}`);
            stateChanged = true;
          }
          if (data.params.LEDSwitch1 !== undefined) {
            led1 = parseInt(data.params.LEDSwitch1) || 0;
            console.log(`💡 [ACTUATOR] LED1 辅助开关已设置为: ${led1 === 1 ? '开启 (ON)' : '关闭 (OFF)'}`);
            stateChanged = true;
          }
          if (data.params.Light_Out_Voltage !== undefined) {
            vol = parseFloat(data.params.Light_Out_Voltage) || 0.0;
            console.log(`⚡ [ACTUATOR] 模拟电压输出已设置为: ${vol.toFixed(2)}V`);
            stateChanged = true;
          }

          // Send Aliyun standard set reply
          const replyPayload = {
            id: data.id,
            code: 200,
            data: {}
          };
          client.publish(replyTopic, JSON.stringify(replyPayload), { qos: 0 });

          // If the state was updated, instantly push the updated properties to Aliyun to sync state immediately
          if (stateChanged) {
            publishProperties('指令响应同步');
          }
        }
      } catch (err) {
        console.error('🔴 [ERR] 解析控制指令失败:', err.message);
      }
    }
  });

  // Start periodic simulation data publishing every 5 seconds
  setInterval(() => {
    temp = +(temp + (Math.random() - 0.5) * 0.4).toFixed(1);
    hum = Math.max(10, Math.min(95, +(hum + (Math.random() - 0.5) * 0.8).toFixed(1)));
    yaw = +((yaw + (Math.random() - 0.5) * 6 + 360) % 360).toFixed(1);
    roll = Math.max(-90, Math.min(90, +(roll + (Math.random() - 0.5) * 4).toFixed(1)));
    pitch = Math.max(-90, Math.min(90, +(pitch + (Math.random() - 0.5) * 3).toFixed(1)));
    adc = Math.max(100, Math.min(1024, adc + Math.floor((Math.random() - 0.5) * 30)));
    
    // Vol is dynamically calculated unless overwritten by cloud voltage settings
    // If not modified recently, we can keep updating vol dynamically, or keep it constant.
    // Let's keep vol updated dynamically based on adc, but if we set it from cloud, it stays.
    // In our case, since the user controls the voltage dimmer, let's keep the set voltage.

    publishProperties('定时上报');
  }, 5000);
});

client.on('error', (err) => {
  console.error('🔴 [ERR] 发生通讯异常:', err.message);
});

client.on('close', () => {
  console.log('🟡 [WARN] 物理终端连接已断开，正在尝试重连...');
});
