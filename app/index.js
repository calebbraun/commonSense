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
	password: config.database.pass,
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

app.get('/', (request, response) => {
	response.render('home', {
		name: 'Caleb'
	})
})


app.post('/', function (req, res) {
	// retrieve user posted data from the body
	const data = req.body
	console.log("got a post request!")

	if (data.access_key == config.access_key) {
		pool.connect(function(err, client, done) {
			if (err) {
				return console.error('Connection error:\n', err)
			}
			Object.keys(data).forEach(function(key) {
				if (key == "access_key") {return}
				var val = data[key]
				/* Table 'commonsense' looks like this:
			 	._______________________________________________________________________________________________.
				| date | tank_top_temp | tank_bottom_temp | ambient_temp | washer1_on | washer2_on | washer3_on |
				|------+---------------+------------------+--------------+------------+------------+------------|
				*/
				client.query('INSERT INTO commonsense (feed, value, date) VALUES ($1, $2, NOW());', [key, val], function(err, result) {
					done()
					if (err) {
						return console.error('Query error:\n', err)
					}
				})
			})
		})
		res.send("Write to database successful.")
	} else {
		ret = 'Successfully posted: ' + data.inputData
		ret += '<br><a href="">Back</a>'
		res.send(ret)
	}
})

app.listen(port, (err) => {
	if (err) {
		return console.log('something bad happened', err)
	}
	console.log(`server is listening on ${port}`)
})
