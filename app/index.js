// app/index.js

const bodyParser = require('body-parser')
const express = require('express')
const exphbs = require('express-handlebars')
const path = require('path')
const { Pool, Client } = require('pg')

const config = require('../config');

const app = express()
const port = config.server.port

const pool = new Pool({
	user: config.database.user,
	host: config.database.host,
	database: config.database.db,
	port: config.database.port
})

app.engine('.hbs', exphbs({
	defaultLayout: 'main',
	extname: '.hbs',
	layoutsDir: path.join(__dirname, 'views/layouts')
}))
app.set('view engine', '.hbs')
app.set('views', path.join(__dirname, 'views'))

app.use(bodyParser.json())
app.use(bodyParser.urlencoded({extended:true}))
//app.use(express.urlencoded());
app.use(express.static('public'))

// ROUTES
var router = express.Router()

router.get('/', function (request, response) {
	pool.connect(function(err, client, done) {
		var washer3Status = 'unable to connect';
		var washer3TimeMessage;
		if (err) {
			console.error('Connection error:\n', err)
		} else {
			client.query('SELECT * FROM washers ORDER BY date DESC LIMIT 1', function(err, result) {
				if (err) throw err
				var w3 = result.rows[0].w3
				if (w3 != null) {
					washer3Status = (w3) ? 'on' : 'off'
					washer3TimeMessage = "It was last turned on "
					var d = new Date(result.rows[0].date)
					washer3TimeMessage += d.toDateString() + "."

				}
				client.end()
				response.render('home', {
					name: 'and welcome to commonSense!',
					washer3Status: washer3Status,
					washer3TimeMessage: washer3TimeMessage
				})
			})
		}
	})
})


router.post('/', function (req, res) {
	// retrieve user posted data from the body
	const data = req.body

	if (data.access_key == config.access_key) {
		pool.connect(function(err, client, done) {
			if (err) {
				console.error('Connection error:\n', err)
				res.send("Write to database unsuccessful.")
			} else {
				var values;
				var query;
				if (data.temps != null) {
					values = [data.temps.tank_top_temp,
							  data.temps.ambient_temp,
							  data.temps.tank_bottom_temp,
							  data.washers.w1,
							  data.washers.w2,
							  data.washers.w3]
					query = 'INSERT INTO commonsense (date, tank_top_temp, tank_bottom_temp, ambient_temp, washer1_on, washer2_on, washer3_on) VALUES (NOW(), $1, $2, $3, $4, $5, $6);'
				} else {
					values = [data.washers.w1,
							  data.w1val,
							  data.washers.w2,
							  data.w2val,
							  data.washers.w3,
							  data.w3val]
					query = 'INSERT INTO washers (date, w1, w1_val, w2, w2_val, w3, w3_val) VALUES (NOW(), $1, $2, $3, $4, $5, $6);'
				}
				client.query(query, values, function(err, result) {
					done()
					if (err) {
						return console.error('Query error:\n', err)
					}
				})
				res.send("Write to database successful.")
			}
		})
	} else {
		ret = 'Successfully posted: ' + data.inputData
		ret += '<br><a href="">Back</a>'
		res.send(ret)
	}
})

router.get('//data', function(req, res) {
	pool.connect(function(err, client, done) {
		if (err) {
			console.error('Connection error:\n', err)
		} else {
			var s = (req.query.s == null) ? 0 : parseInt(req.query.s)
			client.query('SELECT * FROM commonsense ORDER BY date DESC LIMIT 200 OFFSET $1', [s], function(err, result) {
				if (err) throw err
				data = result.rows
				client.end()
				res.render('data', {
					data: data,
					start: s,
					helpers: {
						add: function(lvalue, rvalue) { return parseInt(lvalue) + parseInt(rvalue) + 1}
					}
				})
			})
		}
	})
})

app.use(router)

app.listen(port, (err) => {
	if (err) {
		return console.log('something bad happened', err)
	}
	console.log(`server is listening on ${port}`)
})
