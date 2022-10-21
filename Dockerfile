FROM node:14-alpine

WORKDIR /src
COPY package.json package-lock.json /src/
RUN npm install --production

COPY . /src

CMD ["npm", "start"]
