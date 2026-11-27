const express = require('express');
const app = express();
const bodyParser = require('body-parser')
const routers = require('./backend/rotas_temps','../rotas_user','../mongoConect');
require('dotenv').config()
const cookieParser = require('cookie-parser')
const session = require('express-session')
const mqtt = require('../mqtt_node2');

var fs = require('fs');
const Temps = require('../temps')
const Person = require('../user')
const cors = require('cors')

app.use(cors());

app.use((req,res,next) => {
    console.log("Cors habilitado");
    res.header("Access-Control-Allow-Origin","*");
    res.header("Access-Control-Allow-Header",'Origin, X-Requested-With, Content-Type, Accept, Authorization');
    if (req.method === 'OPTIONS'){
        res.header('Access-Control-Allow-Methods','PUT, POST, PATCH, DELETE, GET');
        res.status(200).send({})
    }
   next()
   })
app.use(cookieParser())
app.use(session({secret: '123456' , token: 'token'}))
app.use(bodyParser.urlencoded({extended: false}))
app.use(bodyParser.json())

app.use(express.json());

app.use(routers);


const PORT = process.env.PORT || 8081 || 5500;
    app.listen(PORT,function(){
        console.log("Servidor Rodando");
        })

  //      "src": "api/index.js",
  //"src": "!{api/**,package.json,middleware.[jt]s}",
