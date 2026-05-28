const dotenv = require('dotenv');
dotenv.config();

const IotClientClass = require('@alicloud/iot20180120').default;
const { Config } = require('@alicloud/openapi-client');

const accessKeyId = process.env.ACCESS_KEY_ID;
const accessKeySecret = process.env.ACCESS_KEY_SECRET;
const regionId = 'cn-shanghai'; // Force Shanghai

const devConfig = {
  productKey: process.env.PRODUCT_KEY || 'YOUR_PRODUCT_KEY',
  deviceName: process.env.DEVICE_NAME || 'YOUR_DEVICE_NAME',
  iotInstanceId: process.env.IOT_INSTANCE_ID || 'YOUR_IOT_INSTANCE_ID'
};

console.log('=========================================');
console.log(' 开始云端数据诊断...');
console.log(` Region: ${regionId}`);
console.log(` Instance ID: ${devConfig.iotInstanceId}`);
console.log(` Product Key: ${devConfig.productKey}`);
console.log(` Device Name: ${devConfig.deviceName}`);
console.log('=========================================');

if (!accessKeyId || !accessKeySecret) {
  console.error('错误: .env 文件中未配置 ACCESS_KEY_ID 或 ACCESS_KEY_SECRET！');
  process.exit(1);
}

const openConfig = new Config({
  accessKeyId,
  accessKeySecret,
});
openConfig.endpoint = `iot.${regionId}.aliyuncs.com`;
const client = new IotClientClass(openConfig);

async function runDiagnostics() {
  try {
    const iot = require('@alicloud/iot20180120');
    
    // 1. Test GetDeviceStatus
    console.log('\n--- 1. 调用 GetDeviceStatus 接口 ---');
    const statusRequest = new iot.GetDeviceStatusRequest({
      iotInstanceId: devConfig.iotInstanceId,
      productKey: devConfig.productKey,
      deviceName: devConfig.deviceName
    });
    const statusResponse = await client.getDeviceStatus(statusRequest);
    console.log('Raw Response Status:', statusResponse.statusCode);
    console.log('Body:', JSON.stringify(statusResponse.body, null, 2));

    // 2. Test QueryDevicePropertyStatus
    console.log('\n--- 2. 调用 QueryDevicePropertyStatus 接口 ---');
    const propertyRequest = new iot.QueryDevicePropertyStatusRequest({
      iotInstanceId: devConfig.iotInstanceId,
      productKey: devConfig.productKey,
      deviceName: devConfig.deviceName
    });
    const propertyResponse = await client.queryDevicePropertyStatus(propertyRequest);
    console.log('Raw Response Status:', propertyResponse.statusCode);
    console.log('Body:', JSON.stringify(propertyResponse.body, null, 2));

  } catch (err) {
    console.error('\n🔴 接口调用发生异常:', err.message);
    if (err.stack) console.error(err.stack);
  }
}

runDiagnostics();
