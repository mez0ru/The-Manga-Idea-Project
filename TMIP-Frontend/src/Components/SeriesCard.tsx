import { useState } from 'react'
import { Series } from '../Home'
import { BASE_URL } from '../api/axios';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Link } from 'react-router-dom';
import Grow from '@mui/material/Grow';

interface Props {
    series: Series;
}

export default function SeriesCard({ series }: Props) {
    const [done, setDone] = useState(false);

    return (
        <Grow in={done}>
            <Card sx={{ maxWidth: 150 }}>
                <CardActionArea component={Link} to={`/series/${series.id}`}>
                    <CardMedia
                        component="img"
                        alt={series.name}
                        onLoad={() => setDone(true)}
                        onError={() => setDone(true)}
                        src={`${BASE_URL}/api/series/${series.id}/cover`}
                    />
                    <CardContent>
                        <Typography gutterBottom variant="body1" sx={{
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            display: "-webkit-box",
                            WebkitLineClamp: "2",
                            WebkitBoxOrient: "vertical",
                        }}>
                            {series.name}
                        </Typography>
                        <br />
                        <Typography gutterBottom variant="body2">
                            {series.chapters} Books
                        </Typography>
                    </CardContent>
                </CardActionArea>
            </Card>
        </Grow>
    )
}
