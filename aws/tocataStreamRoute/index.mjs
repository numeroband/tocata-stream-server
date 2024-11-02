import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { 
  DynamoDBDocumentClient, 
  ScanCommand,
} from "@aws-sdk/lib-dynamodb";
import { 
  ApiGatewayManagementApiClient, 
  PostToConnectionCommand,
  DeleteConnectionCommand,
} from "@aws-sdk/client-apigatewaymanagementapi";

const client = new DynamoDBClient({});
const dynamo = DynamoDBDocumentClient.from(client);
const TableName = "tocata-stream-connections";

export const handler = async (event) => {
  const gateway = new ApiGatewayManagementApiClient({
    endpoint: `https://${event.requestContext.domainName}/${event.requestContext.stage}`,
  });  
  
  let connections = [];
  try {
    const command = new ScanCommand({TableName});
    console.log('Getting connections');
    const dbResponse = await dynamo.send(command);
    connections = dbResponse.Items;
    console.log('Connections received');
  } catch (error) {
    console.error(error);
    return {statusCode: 500};
  }

  const connectionId = event.requestContext.connectionId;
  let sender = null;
  for (const connection of connections) {
    if (connection.connectionId == connectionId) {
      sender = connection.userId;
      break;
    }
  }
  if (!sender) {
    try {
      const input = {ConnectionId: connectionId};
      const command = new DeleteConnectionCommand(input);
      console.log('Deleting connection');
      await gateway.send(command);
      console.log('Connection deleted');
    } catch (error) {
      console.error(error);
      return {statusCode: 500};
    }
  }

  const request = JSON.parse(event.body);
  const {dst} = request;
  const response = JSON.stringify({sender, ...request});
  const promises = [];
  for (const connection of connections) {
    if (connection.connectionId == connectionId || (dst && connection.userId != dst)) {
      continue;
    }
    const command = new PostToConnectionCommand({
      ConnectionId: connection.connectionId,
      Data: response,
    });  
    promises.push(gateway.send(command));
  }

  try {
    console.log('Sending responses...');
    await Promise.all(promises);
    console.log('Responses sent');
  } catch (error) {
    console.error(error);
    return {statusCode: 500};
  }

  return {statusCode: 200};
};
