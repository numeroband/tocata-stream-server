import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { 
  DynamoDBDocumentClient, 
  PutCommand,
  GetCommand,
} from "@aws-sdk/lib-dynamodb";
import { 
  ApiGatewayManagementApiClient, 
  PostToConnectionCommand,
} from "@aws-sdk/client-apigatewaymanagementapi";
import bcrypt from "bcryptjs";

const STATUS_DISCONNECTED = 0
const STATUS_INVALID_USER = 1
const STATUS_INVALID_PASSWORD = 2
const STATUS_CONNECTION_FAILED = 3
const STATUS_CONNECTING = 4
const STATUS_CONNECTED = 5

const client = new DynamoDBClient({});
const dynamo = DynamoDBDocumentClient.from(client);

const login = async (request) => {
  const {type, username, password} = request;
  const name = '';
  const streams = [];
  const status = STATUS_CONNECTED;
  const response = {type, name, streams, status};

  const command = new GetCommand({
    TableName: "tocata-stream-users",
    Key: {
      username,
    },
  });

  let user = null;
  try {
    console.log('Searching user', username);
    const dbResponse = await dynamo.send(command);
    console.log('User searched');
    user = dbResponse.Item;
    console.log(user);
  } catch(error) {
    console.error(error)
    response.status = STATUS_CONNECTION_FAILED;
    return response;
  }
  
  if (!user) {
    console.error('Invalid username');
    response.status = STATUS_INVALID_USER;
    return response;
  }

  console.log('Comparing password')
  const match = await bcrypt.compare(password, user.password);
  console.log('Password compared')
  if (match) {
    response.sender = user.id;
  } else {
    console.error('Invalid password');
    response.status = STATUS_INVALID_PASSWORD;
  }

  return response;
}

export const handler = async (event) => {
  const request = JSON.parse(event.body);
  const response = await login(request);
  if (response.status == STATUS_CONNECTED) {
    const connectionId = event.requestContext.connectionId;
    const userId = response.sender;
    const Item = {
      connectionId,
      userId,
    }
    const command = new PutCommand({
      TableName: "tocata-stream-connections",
      Item,
    });
    try {
      console.log('Adding connection to db', Item);
      await dynamo.send(command);
      console.log('Connection added to db', Item);
    } catch (error) {
      console.error(error);
      response.status = STATUS_CONNECTION_FAILED;
    }    
  }

  const gateway = new ApiGatewayManagementApiClient({
    endpoint: `https://${event.requestContext.domainName}/${event.requestContext.stage}`,
  });  
  const command = new PostToConnectionCommand({
    ConnectionId: event.requestContext.connectionId,
    Data: JSON.stringify(response),
  });
  try {
    console.log('Sending response...');
    await gateway.send(command);
    console.log('Response sent');
  } catch (error) {
    console.error(error);
    return {statusCode: 500};
  }

  return {statusCode: 200};
};
